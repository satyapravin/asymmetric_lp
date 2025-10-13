"""
LP Position Manager for Uniswap V3
Handles adding and removing liquidity positions
"""
import logging
import math
from typing import Dict, Any, Optional, Tuple
from web3 import Web3
from uniswap_client import UniswapV3Client
from config import Config

logger = logging.getLogger(__name__)

class LPPositionManager:
    """Manager for Uniswap V3 LP positions"""
    
    def __init__(self, client: UniswapV3Client = None):
        """Initialize the LP Position Manager"""
        self.client = client or UniswapV3Client()
        self.w3 = self.client.w3
        self.config = self.client.config
        
    def calculate_tick_range(self, current_tick: int, range_percentage: float = 0.1) -> Tuple[int, int]:
        """
        Calculate tick range for liquidity position
        
        Args:
            current_tick: Current pool tick
            range_percentage: Percentage range around current price (0.1 = 10%)
            
        Returns:
            Tuple of (tick_lower, tick_upper)
        """
        # Calculate tick spacing (typically 60 for 0.3% fee tier)
        tick_spacing = 60
        
        # Calculate range in ticks
        tick_range = int(current_tick * range_percentage)
        
        # Round to nearest tick spacing
        tick_lower = ((current_tick - tick_range) // tick_spacing) * tick_spacing
        tick_upper = ((current_tick + tick_range) // tick_spacing) * tick_spacing
        
        return tick_lower, tick_upper
    
    def calculate_liquidity_amounts(
        self,
        token0_amount: int,
        token1_amount: int,
        tick_lower: int,
        tick_upper: int,
        current_tick: int
    ) -> Tuple[int, int]:
        """
        Calculate actual amounts needed for liquidity position
        
        Args:
            token0_amount: Desired amount of token0
            token1_amount: Desired amount of token1
            tick_lower: Lower tick bound
            tick_upper: Upper tick bound
            current_tick: Current pool tick
            
        Returns:
            Tuple of (actual_token0_amount, actual_token1_amount)
        """
        # This is a simplified calculation
        # In practice, you'd need to implement the full Uniswap V3 math
        # For now, we'll use the desired amounts as actual amounts
        
        if current_tick < tick_lower:
            # Only token0 needed
            return token0_amount, 0
        elif current_tick > tick_upper:
            # Only token1 needed
            return 0, token1_amount
        else:
            # Both tokens needed (simplified)
            return token0_amount, token1_amount
    
    def add_liquidity(
        self,
        token0: str,
        token1: str,
        fee: int,
        amount0_desired: int,
        amount1_desired: int,
        tick_lower: int,
        tick_upper: int,
        amount0_min: int = 0,
        amount1_min: int = 0,
        deadline: int = None
    ) -> Dict[str, Any]:
        """
        Add liquidity to a Uniswap V3 pool
        
        Args:
            token0: Address of token0
            token1: Address of token1
            fee: Fee tier (500, 3000, or 10000)
            amount0_desired: Desired amount of token0
            amount1_desired: Desired amount of token1
            tick_lower: Lower tick bound
            tick_upper: Upper tick bound
            amount0_min: Minimum amount of token0 to add
            amount1_min: Minimum amount of token1 to add
            deadline: Transaction deadline (timestamp)
            
        Returns:
            Transaction receipt and position details
        """
        try:
            if deadline is None:
                deadline = int(self.w3.eth.get_block('latest')['timestamp']) + 1800  # 30 minutes
            
            # Get current pool tick
            pool_address = self.client.get_pool_address(token0, token1, fee)
            pool_info = self.client.get_pool_info(pool_address)
            current_tick = pool_info['tick']
            
            # Calculate actual amounts needed
            amount0_actual, amount1_actual = self.calculate_liquidity_amounts(
                amount0_desired, amount1_desired, tick_lower, tick_upper, current_tick
            )
            
            logger.info(f"Adding liquidity: {amount0_actual} token0, {amount1_actual} token1")
            logger.info(f"Tick range: {tick_lower} to {tick_upper} (current: {current_tick})")
            
            # Prepare mint transaction
            mint_params = {
                'token0': token0,
                'token1': token1,
                'fee': fee,
                'tickLower': tick_lower,
                'tickUpper': tick_upper,
                'amount0Desired': amount0_actual,
                'amount1Desired': amount1_actual,
                'amount0Min': amount0_min,
                'amount1Min': amount1_min,
                'recipient': self.client.wallet_address,
                'deadline': deadline
            }
            
            # Build transaction
            transaction = self.client.position_manager.functions.mint(**mint_params).build_transaction({
                'from': self.client.wallet_address,
                'gas': self.client.estimate_gas({
                    'from': self.client.wallet_address,
                    'to': self.config.UNISWAP_V3_POSITION_MANAGER,
                    'data': self.client.position_manager.functions.mint(**mint_params).build_transaction()['data']
                }),
                'gasPrice': self.client.get_gas_price(),
                'nonce': self.w3.eth.get_transaction_count(self.client.wallet_address),
                'value': 0  # Add ETH value if needed for token0 = WETH
            })
            
            # Sign and send transaction
            signed_txn = self.client.account.sign_transaction(transaction)
            tx_hash = self.w3.eth.send_raw_transaction(signed_txn.rawTransaction)
            
            logger.info(f"Transaction sent: {tx_hash.hex()}")
            
            # Wait for confirmation
            receipt = self.w3.eth.wait_for_transaction_receipt(tx_hash)
            
            if receipt.status == 1:
                logger.info("Liquidity added successfully!")
                
                # Extract position details from transaction logs
                position_details = self._extract_position_from_receipt(receipt)
                
                return {
                    'success': True,
                    'transaction_hash': tx_hash.hex(),
                    'receipt': receipt,
                    'position_details': position_details
                }
            else:
                logger.error("Transaction failed!")
                return {
                    'success': False,
                    'transaction_hash': tx_hash.hex(),
                    'receipt': receipt,
                    'error': 'Transaction failed'
                }
                
        except Exception as e:
            logger.error(f"Error adding liquidity: {e}")
            return {
                'success': False,
                'error': str(e)
            }
    
    def remove_liquidity(
        self,
        token_id: int,
        liquidity_amount: int,
        amount0_min: int = 0,
        amount1_min: int = 0,
        deadline: int = None
    ) -> Dict[str, Any]:
        """
        Remove liquidity from a position
        
        Args:
            token_id: NFT token ID of the position
            liquidity_amount: Amount of liquidity to remove
            amount0_min: Minimum amount of token0 to receive
            amount1_min: Minimum amount of token1 to receive
            deadline: Transaction deadline (timestamp)
            
        Returns:
            Transaction receipt and amounts received
        """
        try:
            if deadline is None:
                deadline = int(self.w3.eth.get_block('latest')['timestamp']) + 1800  # 30 minutes
            
            logger.info(f"Removing {liquidity_amount} liquidity from position {token_id}")
            
            # Prepare decrease liquidity transaction
            decrease_params = {
                'tokenId': token_id,
                'liquidity': liquidity_amount,
                'amount0Min': amount0_min,
                'amount1Min': amount1_min,
                'deadline': deadline
            }
            
            # Build transaction
            transaction = self.client.position_manager.functions.decreaseLiquidity(**decrease_params).build_transaction({
                'from': self.client.wallet_address,
                'gas': self.client.estimate_gas({
                    'from': self.client.wallet_address,
                    'to': self.config.UNISWAP_V3_POSITION_MANAGER,
                    'data': self.client.position_manager.functions.decreaseLiquidity(**decrease_params).build_transaction()['data']
                }),
                'gasPrice': self.client.get_gas_price(),
                'nonce': self.w3.eth.get_transaction_count(self.client.wallet_address),
                'value': 0
            })
            
            # Sign and send transaction
            signed_txn = self.client.account.sign_transaction(transaction)
            tx_hash = self.w3.eth.send_raw_transaction(signed_txn.rawTransaction)
            
            logger.info(f"Transaction sent: {tx_hash.hex()}")
            
            # Wait for confirmation
            receipt = self.w3.eth.wait_for_transaction_receipt(tx_hash)
            
            if receipt.status == 1:
                logger.info("Liquidity removed successfully!")
                
                # Extract amounts from transaction logs
                amounts_received = self._extract_amounts_from_receipt(receipt)
                
                return {
                    'success': True,
                    'transaction_hash': tx_hash.hex(),
                    'receipt': receipt,
                    'amounts_received': amounts_received
                }
            else:
                logger.error("Transaction failed!")
                return {
                    'success': False,
                    'transaction_hash': tx_hash.hex(),
                    'receipt': receipt,
                    'error': 'Transaction failed'
                }
                
        except Exception as e:
            logger.error(f"Error removing liquidity: {e}")
            return {
                'success': False,
                'error': str(e)
            }
    
    def get_position_value(self, token_id: int) -> Dict[str, Any]:
        """Get current value of a position"""
        try:
            position_info = self.client.get_position_info(token_id)
            
            # Get pool info
            pool_address = self.client.get_pool_address(
                position_info['token0'],
                position_info['token1'],
                position_info['fee']
            )
            pool_info = self.client.get_pool_info(pool_address)
            
            # Calculate position value (simplified)
            # In practice, you'd need to implement the full Uniswap V3 math
            current_tick = pool_info['tick']
            tick_lower = position_info['tick_lower']
            tick_upper = position_info['tick_upper']
            liquidity = position_info['liquidity']
            
            return {
                'token_id': token_id,
                'token0': position_info['token0'],
                'token1': position_info['token1'],
                'fee': position_info['fee'],
                'tick_range': (tick_lower, tick_upper),
                'current_tick': current_tick,
                'liquidity': liquidity,
                'tokens_owed0': position_info['tokens_owed0'],
                'tokens_owed1': position_info['tokens_owed1'],
                'pool_address': pool_address
            }
            
        except Exception as e:
            logger.error(f"Error getting position value: {e}")
            raise
    
    def _extract_position_from_receipt(self, receipt: Dict[str, Any]) -> Dict[str, Any]:
        """Extract position details from transaction receipt"""
        # This is a simplified extraction
        # In practice, you'd parse the transaction logs to get the exact values
        return {
            'token_id': 'extracted_from_logs',
            'liquidity': 'extracted_from_logs',
            'amount0': 'extracted_from_logs',
            'amount1': 'extracted_from_logs'
        }
    
    def _extract_amounts_from_receipt(self, receipt: Dict[str, Any]) -> Dict[str, Any]:
        """Extract amounts from transaction receipt"""
        # This is a simplified extraction
        # In practice, you'd parse the transaction logs to get the exact values
        return {
            'amount0': 'extracted_from_logs',
            'amount1': 'extracted_from_logs'
        }
