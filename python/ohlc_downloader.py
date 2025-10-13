#!/usr/bin/env python3
"""
OHLC Data Downloader for Blockchain Data
Downloads 1-second OHLC bars from Uniswap V3 pools on various chains.
"""

import asyncio
import logging
import pandas as pd
import numpy as np
from datetime import datetime, timedelta, timezone
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass
import json
import time
from web3 import Web3
from web3.middleware import geth_poa_middleware
import requests
from config import Config
from uniswap_client import UniswapV3Client

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

@dataclass
class OHLCBar:
    """Represents a single OHLC bar"""
    timestamp: datetime
    open: float
    high: float
    low: float
    close: float
    volume: float
    tick: int
    sqrt_price_x96: int

class OHLCDataDownloader:
    """Downloads OHLC data from Uniswap V3 pools"""
    
    def __init__(self, config: Config = None):
        """Initialize the OHLC data downloader"""
        self.config = config or Config()
        self.uniswap_client = UniswapV3Client(self.config, read_only=True)
        self.w3 = self.uniswap_client.w3
        
        # Helper function to checksum addresses
        self.checksum_address = lambda addr: self.w3.to_checksum_address(addr) if addr else None
        
        # Add PoA middleware for chains like Polygon, Arbitrum, etc.
        if self.config.CHAIN_ID in [137, 42161, 10, 8453]:  # Polygon, Arbitrum, Optimism, Base
            self.w3.middleware_onion.inject(geth_poa_middleware, layer=0)
        
        # Pool contract ABI for price data
        self.pool_abi = [
            {
                "inputs": [],
                "name": "slot0",
                "outputs": [
                    {"internalType": "uint160", "name": "sqrtPriceX96", "type": "uint160"},
                    {"internalType": "int24", "name": "tick", "type": "int24"},
                    {"internalType": "uint16", "name": "observationIndex", "type": "uint16"},
                    {"internalType": "uint16", "name": "observationCardinality", "type": "uint16"},
                    {"internalType": "uint16", "name": "observationCardinalityNext", "type": "uint16"},
                    {"internalType": "uint8", "name": "feeProtocol", "type": "uint8"},
                    {"internalType": "bool", "name": "unlocked", "type": "bool"}
                ],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [],
                "name": "token0",
                "outputs": [{"internalType": "address", "name": "", "type": "address"}],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [],
                "name": "token1",
                "outputs": [{"internalType": "address", "name": "", "type": "address"}],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [],
                "name": "fee",
                "outputs": [{"internalType": "uint24", "name": "", "type": "uint24"}],
                "stateMutability": "view",
                "type": "function"
            }
        ]
        
        # Event ABI for Swap events
        self.swap_event_abi = {
            "anonymous": False,
            "inputs": [
                {"indexed": True, "internalType": "address", "name": "sender", "type": "address"},
                {"indexed": True, "internalType": "address", "name": "recipient", "type": "address"},
                {"indexed": False, "internalType": "int256", "name": "amount0", "type": "int256"},
                {"indexed": False, "internalType": "int256", "name": "amount1", "type": "int256"},
                {"indexed": False, "internalType": "uint160", "name": "sqrtPriceX96", "type": "uint160"},
                {"indexed": False, "internalType": "uint128", "name": "liquidity", "type": "uint128"},
                {"indexed": False, "internalType": "int24", "name": "tick", "type": "int24"}
            ],
            "name": "Swap",
            "type": "event"
        }
        
        logger.info(f"Initialized OHLC downloader for {self.config.CHAIN_NAME}")
    
    def get_pool_address(self, token0: str, token1: str, fee: int) -> str:
        """Get pool address for token pair and fee tier"""
        # Checksum addresses before querying
        token0_checksummed = self.checksum_address(token0)
        token1_checksummed = self.checksum_address(token1)
        return self.uniswap_client.get_pool_address(token0_checksummed, token1_checksummed, fee)
    
    def get_pool_contract(self, pool_address: str):
        """Get pool contract instance"""
        pool_address_checksummed = self.checksum_address(pool_address)
        return self.w3.eth.contract(address=pool_address_checksummed, abi=self.pool_abi)
    
    def sqrt_price_x96_to_price(self, sqrt_price_x96: int, token0_decimals: int, token1_decimals: int) -> float:
        """Convert sqrt price X96 to human-readable price"""
        # Price = (sqrt_price_x96 / 2^96)^2 * (10^token0_decimals / 10^token1_decimals)
        price = (sqrt_price_x96 / (2**96))**2
        price *= (10**token0_decimals) / (10**token1_decimals)
        return price
    
    def get_current_price(self, pool_address: str) -> Tuple[float, int, int]:
        """Get current price from pool"""
        pool = self.get_pool_contract(pool_address)
        slot0 = pool.functions.slot0().call()
        sqrt_price_x96 = slot0[0]
        tick = slot0[1]
        
        # Get token addresses and decimals
        token0_address = pool.functions.token0().call()
        token1_address = pool.functions.token1().call()
        
        token0_decimals = self.uniswap_client.get_token_decimals(token0_address)
        token1_decimals = self.uniswap_client.get_token_decimals(token1_address)
        
        price = self.sqrt_price_x96_to_price(sqrt_price_x96, token0_decimals, token1_decimals)
        
        return price, tick, sqrt_price_x96
    
    def get_swap_events(self, pool_address: str, from_block: int, to_block: int, chunk_size: int = 1000) -> List[Dict]:
        """Get swap events from pool within block range"""
        pool_address_checksummed = self.checksum_address(pool_address)
        
        # Swap event signature
        swap_event_signature = self.w3.keccak(text="Swap(address,address,int256,int256,uint160,uint128,int24)").hex()
        
        try:
            all_events = []
            
            # Split into chunks to avoid RPC limitations
            for chunk_start in range(from_block, to_block + 1, chunk_size):
                chunk_end = min(chunk_start + chunk_size - 1, to_block)
                
                logger.info(f"Fetching events from block {chunk_start} to {chunk_end}...")
                
                # Get events using event logs
                events = self.w3.eth.get_logs({
                    'fromBlock': chunk_start,
                    'toBlock': chunk_end,
                    'address': pool_address_checksummed,
                    'topics': [swap_event_signature]
                })
                
                all_events.extend(events)
                
                # Add a small delay to avoid rate limiting
                time.sleep(0.1)
            
            logger.info(f"Total events fetched: {len(all_events)}")
            
            swap_data = []
            for event in all_events:
                # Decode event data
                data = event['data']
                # Convert HexBytes to hex string if needed
                if hasattr(data, 'hex'):
                    data = data.hex()
                # Remove 0x prefix if present
                if data.startswith('0x'):
                    data = data[2:]
                    
                # Parse the data field (contains amount0, amount1, sqrtPriceX96, liquidity, tick)
                amount0 = int.from_bytes(bytes.fromhex(data[0:64]), 'big', signed=True)
                amount1 = int.from_bytes(bytes.fromhex(data[64:128]), 'big', signed=True)
                sqrt_price_x96 = int(data[128:192], 16)
                liquidity = int(data[192:256], 16)
                tick = int.from_bytes(bytes.fromhex(data[256:320]), 'big', signed=True)
                
                swap_data.append({
                    'block_number': event['blockNumber'],
                    'transaction_hash': event['transactionHash'].hex(),
                    'timestamp': self.w3.eth.get_block(event['blockNumber'])['timestamp'],
                    'sqrt_price_x96': sqrt_price_x96,
                    'tick': tick,
                    'amount0': amount0,
                    'amount1': amount1,
                    'liquidity': liquidity
                })
            
            return swap_data
            
        except Exception as e:
            logger.error(f"Error fetching swap events: {e}")
            return []
    
    def get_block_range_for_timeframe(self, start_time: datetime, end_time: datetime) -> Tuple[int, int]:
        """Get block range for given timeframe"""
        # Convert to timestamps
        start_timestamp = int(start_time.timestamp())
        end_timestamp = int(end_time.timestamp())
        
        # Get latest block
        latest_block = self.w3.eth.block_number
        latest_timestamp = self.w3.eth.get_block(latest_block)['timestamp']
        
        # Estimate block range (rough approximation)
        # Most chains have ~12-15 second block times
        avg_block_time = 12  # seconds
        blocks_per_second = 1 / avg_block_time
        
        # Estimate start and end blocks
        time_diff = latest_timestamp - start_timestamp
        estimated_start_block = latest_block - int(time_diff * blocks_per_second)
        
        time_diff_end = latest_timestamp - end_timestamp
        estimated_end_block = latest_block - int(time_diff_end * blocks_per_second)
        
        # Ensure we don't go below block 0
        start_block = max(0, estimated_start_block)
        end_block = max(start_block, estimated_end_block)
        
        logger.info(f"Estimated block range: {start_block} to {end_block}")
        return start_block, end_block
    
    def create_ohlc_bars_from_swaps(self, swap_data: List[Dict], pool_address: str, 
                                   interval_seconds: int = 1) -> List[OHLCBar]:
        """Create OHLC bars from swap events"""
        if not swap_data:
            return []
        
        # Get token info for price conversion
        pool = self.get_pool_contract(pool_address)
        token0_address = pool.functions.token0().call()
        token1_address = pool.functions.token1().call()
        
        token0_decimals = self.uniswap_client.get_token_decimals(token0_address)
        token1_decimals = self.uniswap_client.get_token_decimals(token1_address)
        
        # Sort swaps by timestamp
        swap_data.sort(key=lambda x: x['timestamp'])
        
        # Group swaps by time intervals
        bars = {}
        
        for swap in swap_data:
            timestamp = datetime.fromtimestamp(swap['timestamp'], tz=timezone.utc)
            
            # Round down to interval
            interval_start = timestamp.replace(second=(timestamp.second // interval_seconds) * interval_seconds, 
                                             microsecond=0)
            
            # Convert sqrt price to actual price
            price = self.sqrt_price_x96_to_price(swap['sqrt_price_x96'], token0_decimals, token1_decimals)
            
            # Calculate volume (absolute value of amount changes)
            volume = abs(float(swap['amount0'])) + abs(float(swap['amount1']))
            
            if interval_start not in bars:
                bars[interval_start] = {
                    'open': price,
                    'high': price,
                    'low': price,
                    'close': price,
                    'volume': volume,
                    'tick': swap['tick'],
                    'sqrt_price_x96': swap['sqrt_price_x96']
                }
            else:
                bar = bars[interval_start]
                bar['high'] = max(bar['high'], price)
                bar['low'] = min(bar['low'], price)
                bar['close'] = price
                bar['volume'] += volume
                bar['tick'] = swap['tick']  # Use latest tick
                bar['sqrt_price_x96'] = swap['sqrt_price_x96']  # Use latest sqrt price
        
        # Convert to OHLCBar objects
        ohlc_bars = []
        for timestamp, data in sorted(bars.items()):
            ohlc_bars.append(OHLCBar(
                timestamp=timestamp,
                open=data['open'],
                high=data['high'],
                low=data['low'],
                close=data['close'],
                volume=data['volume'],
                tick=data['tick'],
                sqrt_price_x96=data['sqrt_price_x96']
            ))
        
        return ohlc_bars
    
    def download_ohlc_data(self, token0: str, token1: str, fee: int, 
                          start_time: datetime, end_time: datetime,
                          interval_seconds: int = 1) -> List[OHLCBar]:
        """Download OHLC data for specified timeframe"""
        logger.info(f"Downloading OHLC data for {token0}/{token1} (fee: {fee})")
        logger.info(f"Timeframe: {start_time} to {end_time}")
        logger.info(f"Interval: {interval_seconds} seconds")
        
        # Get pool address
        pool_address = self.get_pool_address(token0, token1, fee)
        logger.info(f"Pool address: {pool_address}")
        
        # Get block range
        start_block, end_block = self.get_block_range_for_timeframe(start_time, end_time)
        
        # Get swap events
        logger.info("Fetching swap events...")
        swap_data = self.get_swap_events(pool_address, start_block, end_block)
        logger.info(f"Found {len(swap_data)} swap events")
        
        if not swap_data:
            logger.warning("No swap events found. Creating single bar with current price.")
            # Create a single bar with current price
            current_price, current_tick, current_sqrt_price = self.get_current_price(pool_address)
            return [OHLCBar(
                timestamp=start_time,
                open=current_price,
                high=current_price,
                low=current_price,
                close=current_price,
                volume=0.0,
                tick=current_tick,
                sqrt_price_x96=current_sqrt_price
            )]
        
        # Create OHLC bars
        logger.info("Creating OHLC bars...")
        ohlc_bars = self.create_ohlc_bars_from_swaps(swap_data, pool_address, interval_seconds)
        
        logger.info(f"Created {len(ohlc_bars)} OHLC bars")
        return ohlc_bars
    
    def save_ohlc_data(self, ohlc_bars: List[OHLCBar], filename: str):
        """Save OHLC data to CSV file"""
        if not ohlc_bars:
            logger.warning("No OHLC data to save")
            return
        
        # Convert to DataFrame
        data = []
        for bar in ohlc_bars:
            data.append({
                'timestamp': bar.timestamp.strftime('%Y-%m-%d %H:%M:%S'),
                'open': round(bar.open, 8),
                'high': round(bar.high, 8),
                'low': round(bar.low, 8),
                'close': round(bar.close, 8),
                'volume': round(bar.volume, 2)
            })
        
        df = pd.DataFrame(data)
        df.to_csv(filename, index=False)
        
        logger.info(f"Saved {len(ohlc_bars)} OHLC bars to {filename}")
        logger.info(f"Date range: {df['timestamp'].iloc[0]} to {df['timestamp'].iloc[-1]}")
        logger.info(f"Price range: ${df['low'].min():.8f} - ${df['high'].max():.8f}")

def main():
    """Main function for command line usage"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Download OHLC data from Uniswap V3 pools')
    parser.add_argument('--token0', required=True, help='Token0 address')
    parser.add_argument('--token1', required=True, help='Token1 address')
    parser.add_argument('--fee', type=int, default=3000, help='Fee tier (500, 3000, 10000)')
    parser.add_argument('--start-time', required=True, help='Start time (YYYY-MM-DD HH:MM:SS)')
    parser.add_argument('--end-time', required=True, help='End time (YYYY-MM-DD HH:MM:SS)')
    parser.add_argument('--interval', type=int, default=1, help='Interval in seconds (default: 1)')
    parser.add_argument('--output', required=True, help='Output CSV file')
    
    args = parser.parse_args()
    
    # Parse timestamps
    start_time = datetime.strptime(args.start_time, '%Y-%m-%d %H:%M:%S')
    end_time = datetime.strptime(args.end_time, '%Y-%m-%d %H:%M:%S')
    
    # Add timezone info
    start_time = start_time.replace(tzinfo=timezone.utc)
    end_time = end_time.replace(tzinfo=timezone.utc)
    
    try:
        # Initialize downloader
        config = Config()
        downloader = OHLCDataDownloader(config)
        
        # Download data
        ohlc_bars = downloader.download_ohlc_data(
            token0=args.token0,
            token1=args.token1,
            fee=args.fee,
            start_time=start_time,
            end_time=end_time,
            interval_seconds=args.interval
        )
        
        # Save data
        downloader.save_ohlc_data(ohlc_bars, args.output)
        
        print(f"✅ Successfully downloaded {len(ohlc_bars)} OHLC bars to {args.output}")
        
    except Exception as e:
        logger.error(f"Error downloading OHLC data: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    exit(main())
