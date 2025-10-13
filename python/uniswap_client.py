"""
Uniswap V3 client for interacting with the protocol.
Handles pool queries, position management, and transactions.
"""
import logging
from typing import Dict, Any, Optional, Tuple
from web3 import Web3
from eth_account import Account
from hexbytes import HexBytes
from config import Config

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class UniswapV3Client:
    """Client for interacting with Uniswap V3 protocol"""
    
    def __init__(self, config: Config = None, read_only: bool = False):
        """Initialize the Uniswap V3 client"""
        self.config = config or Config()
        
        # Initialize Web3 connection
        self.w3 = Web3(Web3.HTTPProvider(self.config.ETHEREUM_RPC_URL))
        if not self.w3.is_connected():
            raise ConnectionError("Failed to connect to Ethereum network")
        
        # Initialize account only if not in read-only mode
        if not read_only and self.config.PRIVATE_KEY:
            self.account = Account.from_key(self.config.PRIVATE_KEY)
            self.wallet_address = self.account.address
        else:
            self.account = None
            self.wallet_address = None
        
        # Validate config only for non-read-only mode
        if not read_only:
            self.config.validate_config()
        
        # Get chain info from environment
        self.chain_info = self.config.get_chain_info()
        
        logger.info(f"Connected to {self.chain_info['chain_name']} (Chain ID: {self.chain_info['chain_id']})")
        if self.wallet_address:
            logger.info(f"Wallet: {self.wallet_address}")
        
        # Use contract addresses from environment
        self.factory_address = self.config.UNISWAP_V3_FACTORY
        self.position_manager_address = self.config.UNISWAP_V3_POSITION_MANAGER
        self.router_address = self.config.UNISWAP_V3_ROUTER
        
        # Cache for token decimals
        self.token_decimals_cache = {}
        
        # Contract ABIs (simplified versions)
        self.position_manager_abi = self._get_position_manager_abi()
        self.factory_abi = self._get_factory_abi()
        self.pool_abi = self._get_pool_abi()
        self.erc20_abi = self._get_erc20_abi()
        
        # Initialize contracts
        self.position_manager = self.w3.eth.contract(
            address=self.position_manager_address,
            abi=self.position_manager_abi
        )
        
        self.factory = self.w3.eth.contract(
            address=self.factory_address,
            abi=self.factory_abi
        )
    
    def _get_position_manager_abi(self) -> list:
        """Get Position Manager ABI"""
        return [
            {
                "inputs": [
                    {"internalType": "address", "name": "token0", "type": "address"},
                    {"internalType": "address", "name": "token1", "type": "address"},
                    {"internalType": "uint24", "name": "fee", "type": "uint24"},
                    {"internalType": "int24", "name": "tickLower", "type": "int24"},
                    {"internalType": "int24", "name": "tickUpper", "type": "int24"},
                    {"internalType": "uint256", "name": "amount0Desired", "type": "uint256"},
                    {"internalType": "uint256", "name": "amount1Desired", "type": "uint256"},
                    {"internalType": "uint256", "name": "amount0Min", "type": "uint256"},
                    {"internalType": "uint256", "name": "amount1Min", "type": "uint256"},
                    {"internalType": "address", "name": "recipient", "type": "address"},
                    {"internalType": "uint256", "name": "deadline", "type": "uint256"}
                ],
                "name": "mint",
                "outputs": [
                    {"internalType": "uint256", "name": "tokenId", "type": "uint256"},
                    {"internalType": "uint128", "name": "liquidity", "type": "uint128"},
                    {"internalType": "uint256", "name": "amount0", "type": "uint256"},
                    {"internalType": "uint256", "name": "amount1", "type": "uint256"}
                ],
                "stateMutability": "payable",
                "type": "function"
            },
            {
                "inputs": [
                    {"internalType": "uint256", "name": "tokenId", "type": "uint256"},
                    {"internalType": "uint128", "name": "liquidity", "type": "uint128"},
                    {"internalType": "uint256", "name": "amount0Min", "type": "uint256"},
                    {"internalType": "uint256", "name": "amount1Min", "type": "uint256"},
                    {"internalType": "uint256", "name": "deadline", "type": "uint256"}
                ],
                "name": "decreaseLiquidity",
                "outputs": [
                    {"internalType": "uint256", "name": "amount0", "type": "uint256"},
                    {"internalType": "uint256", "name": "amount1", "type": "uint256"}
                ],
                "stateMutability": "payable",
                "type": "function"
            },
            {
                "inputs": [
                    {"internalType": "uint256", "name": "tokenId", "type": "uint256"}
                ],
                "name": "positions",
                "outputs": [
                    {"internalType": "uint96", "name": "nonce", "type": "uint96"},
                    {"internalType": "address", "name": "operator", "type": "address"},
                    {"internalType": "address", "name": "token0", "type": "address"},
                    {"internalType": "address", "name": "token1", "type": "address"},
                    {"internalType": "uint24", "name": "fee", "type": "uint24"},
                    {"internalType": "int24", "name": "tickLower", "type": "int24"},
                    {"internalType": "int24", "name": "tickUpper", "type": "int24"},
                    {"internalType": "uint128", "name": "liquidity", "type": "uint128"},
                    {"internalType": "uint256", "name": "feeGrowthInside0LastX128", "type": "uint256"},
                    {"internalType": "uint256", "name": "feeGrowthInside1LastX128", "type": "uint256"},
                    {"internalType": "uint128", "name": "tokensOwed0", "type": "uint128"},
                    {"internalType": "uint128", "name": "tokensOwed1", "type": "uint128"}
                ],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [
                    {"internalType": "uint256", "name": "tokenId", "type": "uint256"},
                    {"internalType": "address", "name": "recipient", "type": "address"},
                    {"internalType": "uint128", "name": "amount0Max", "type": "uint128"},
                    {"internalType": "uint128", "name": "amount1Max", "type": "uint128"}
                ],
                "name": "collect",
                "outputs": [
                    {"internalType": "uint256", "name": "amount0", "type": "uint256"},
                    {"internalType": "uint256", "name": "amount1", "type": "uint256"}
                ],
                "stateMutability": "payable",
                "type": "function"
            }
        ]
    
    def _get_factory_abi(self) -> list:
        """Get Factory ABI"""
        return [
            {
                "inputs": [
                    {"internalType": "address", "name": "tokenA", "type": "address"},
                    {"internalType": "address", "name": "tokenB", "type": "address"},
                    {"internalType": "uint24", "name": "fee", "type": "uint24"}
                ],
                "name": "getPool",
                "outputs": [
                    {"internalType": "address", "name": "pool", "type": "address"}
                ],
                "stateMutability": "view",
                "type": "function"
            }
        ]
    
    def _get_pool_abi(self) -> list:
        """Get Pool ABI"""
        return [
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
                "outputs": [
                    {"internalType": "address", "name": "", "type": "address"}
                ],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [],
                "name": "token1",
                "outputs": [
                    {"internalType": "address", "name": "", "type": "address"}
                ],
                "stateMutability": "view",
                "type": "function"
            }
        ]
    
    def _get_erc20_abi(self) -> list:
        """Get ERC20 ABI for token interactions"""
        return [
            {
                "inputs": [],
                "name": "decimals",
                "outputs": [
                    {"internalType": "uint8", "name": "", "type": "uint8"}
                ],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [],
                "name": "symbol",
                "outputs": [
                    {"internalType": "string", "name": "", "type": "string"}
                ],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [],
                "name": "name",
                "outputs": [
                    {"internalType": "string", "name": "", "type": "string"}
                ],
                "stateMutability": "view",
                "type": "function"
            },
            {
                "inputs": [
                    {"internalType": "address", "name": "account", "type": "address"}
                ],
                "name": "balanceOf",
                "outputs": [
                    {"internalType": "uint256", "name": "", "type": "uint256"}
                ],
                "stateMutability": "view",
                "type": "function"
            }
        ]
    
    def get_pool_address(self, token0: str, token1: str, fee: int) -> str:
        """Get the pool address for a given token pair and fee tier"""
        try:
            # Ensure addresses are checksummed
            token0_checksummed = self.w3.to_checksum_address(token0)
            token1_checksummed = self.w3.to_checksum_address(token1)
            
            pool_address = self.factory.functions.getPool(token0_checksummed, token1_checksummed, fee).call()
            if pool_address == "0x0000000000000000000000000000000000000000":
                raise ValueError(f"No pool found for tokens {token0}/{token1} with fee {fee}")
            return pool_address
        except Exception as e:
            logger.error(f"Error getting pool address: {e}")
            raise
    
    def get_token_decimals(self, token_address: str) -> int:
        """
        Get token decimals from blockchain
        
        Args:
            token_address: Token contract address
            
        Returns:
            Number of decimals for the token
        """
        try:
            # Check cache first
            if token_address in self.token_decimals_cache:
                return self.token_decimals_cache[token_address]
            
            # Ensure address is checksummed
            token_address_checksummed = self.w3.to_checksum_address(token_address)
            
            # Create ERC20 contract instance
            token_contract = self.w3.eth.contract(
                address=token_address_checksummed,
                abi=self.erc20_abi
            )
            
            # Fetch decimals from blockchain
            decimals = token_contract.functions.decimals().call()
            
            # Cache the result
            self.token_decimals_cache[token_address] = decimals
            
            logger.debug(f"Fetched decimals for token {token_address}: {decimals}")
            return decimals
            
        except Exception as e:
            logger.error(f"Error fetching decimals for token {token_address}: {e}")
            # Return default decimals based on common patterns
            if token_address.lower() == self.config.WETH_ADDRESS.lower():
                return 18
            elif 'usdc' in token_address.lower() or 'usdt' in token_address.lower():
                return 6
            elif 'wbtc' in token_address.lower():
                return 8
            else:
                return 18  # Default to 18 decimals
    
    def get_token_info(self, token_address: str) -> Dict[str, Any]:
        """
        Get comprehensive token information from blockchain
        
        Args:
            token_address: Token contract address
            
        Returns:
            Dictionary with token information
        """
        try:
            # Create ERC20 contract instance
            token_contract = self.w3.eth.contract(
                address=token_address,
                abi=self.erc20_abi
            )
            
            # Fetch token information
            decimals = token_contract.functions.decimals().call()
            symbol = token_contract.functions.symbol().call()
            name = token_contract.functions.name().call()
            
            return {
                'address': token_address,
                'decimals': decimals,
                'symbol': symbol,
                'name': name
            }
            
        except Exception as e:
            logger.error(f"Error fetching token info for {token_address}: {e}")
            return {
                'address': token_address,
                'decimals': 18,  # Default
                'symbol': 'UNKNOWN',
                'name': 'Unknown Token'
            }
    
    def get_pool_info(self, pool_address: str) -> Dict[str, Any]:
        """Get current pool information"""
        try:
            pool = self.w3.eth.contract(address=pool_address, abi=self.pool_abi)
            slot0 = pool.functions.slot0().call()
            token0 = pool.functions.token0().call()
            token1 = pool.functions.token1().call()
            
            return {
                'pool_address': pool_address,
                'token0': token0,
                'token1': token1,
                'sqrt_price_x96': slot0[0],
                'tick': slot0[1],
                'observation_index': slot0[2],
                'observation_cardinality': slot0[3],
                'observation_cardinality_next': slot0[4],
                'fee_protocol': slot0[5],
                'unlocked': slot0[6]
            }
        except Exception as e:
            logger.error(f"Error getting pool info: {e}")
            raise
    
    def get_position_info(self, token_id: int) -> Dict[str, Any]:
        """Get information about a specific position"""
        try:
            position = self.position_manager.functions.positions(token_id).call()
            return {
                'token_id': token_id,
                'nonce': position[0],
                'operator': position[1],
                'token0': position[2],
                'token1': position[3],
                'fee': position[4],
                'tick_lower': position[5],
                'tick_upper': position[6],
                'liquidity': position[7],
                'fee_growth_inside0_last_x128': position[8],
                'fee_growth_inside1_last_x128': position[9],
                'tokens_owed0': position[10],
                'tokens_owed1': position[11]
            }
        except Exception as e:
            logger.error(f"Error getting position info: {e}")
            raise
    
    def get_gas_price(self) -> int:
        """Get current gas price dynamically from the network"""
        try:
            # Try to get gas price from the network
            gas_price = self.w3.eth.gas_price
            
            # For EIP-1559 chains, also try to get priority fee
            if hasattr(self.w3.eth, 'max_priority_fee'):
                try:
                    priority_fee = self.w3.eth.max_priority_fee
                    # Use base fee + priority fee for EIP-1559
                    base_fee = self.w3.eth.get_block('latest')['baseFeePerGas']
                    gas_price = base_fee + priority_fee
                except Exception:
                    # Fallback to regular gas price
                    pass
            
            logger.info(f"Dynamic gas price: {gas_price} wei ({self.w3.from_wei(gas_price, 'gwei')} gwei)")
            return gas_price
            
        except Exception as e:
            logger.error(f"Error getting dynamic gas price: {e}")
            # Fallback to a reasonable default based on chain
            fallback_gas_prices = {
                'ethereum': self.w3.to_wei(20, 'gwei'),
                'polygon': self.w3.to_wei(30, 'gwei'),
                'arbitrum': self.w3.to_wei(0.1, 'gwei'),
                'optimism': self.w3.to_wei(0.001, 'gwei'),
                'base': self.w3.to_wei(0.001, 'gwei')
            }
            fallback_price = fallback_gas_prices.get(self.chain_name, self.w3.to_wei(20, 'gwei'))
            logger.warning(f"Using fallback gas price: {fallback_price} wei")
            return fallback_price
    
    def get_gas_price_with_buffer(self, buffer_percentage: float = 0.1) -> int:
        """
        Get gas price with a buffer for faster confirmation
        
        Args:
            buffer_percentage: Percentage to increase gas price (default 10%)
            
        Returns:
            Gas price with buffer
        """
        base_gas_price = self.get_gas_price()
        buffered_price = int(base_gas_price * (1 + buffer_percentage))
        
        logger.info(f"Gas price with {buffer_percentage*100}% buffer: {buffered_price} wei")
        return buffered_price
    
    def estimate_gas(self, transaction: Dict[str, Any]) -> int:
        """Estimate gas for a transaction"""
        try:
            gas_estimate = self.w3.eth.estimate_gas(transaction)
            return min(gas_estimate, self.config.MAX_GAS_LIMIT)
        except Exception as e:
            logger.error(f"Error estimating gas: {e}")
            return self.config.MAX_GAS_LIMIT
