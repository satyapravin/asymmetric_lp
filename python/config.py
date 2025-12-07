"""
Configuration management for AsymmetricLP.
Loads settings from environment variables.
"""
import os
from typing import Dict, Any, Optional
from dotenv import load_dotenv

# Load base environment variables (production config)
load_dotenv()

# Function to load backtest config (called explicitly when needed)
def load_backtest_config():
    """Load backtest-specific configuration from .env.backtest or env.backtest.example"""
    if os.path.exists('.env.backtest'):
        load_dotenv('.env.backtest', override=False)  # Don't override production values
        return True
    elif os.path.exists('env.backtest.example'):
        # For development/testing, allow loading from example file
        load_dotenv('env.backtest.example', override=False)
        return True
    return False

class Config:
    """Configuration class for Uniswap V3 operations"""
    
    # Network settings
    ETHEREUM_RPC_URL = os.getenv('ETHEREUM_RPC_URL', 'https://mainnet.infura.io/v3/YOUR_PROJECT_ID')
    
    # Private key handling - ONLY environment variable references allowed
    PRIVATE_KEY = os.getenv('PRIVATE_KEY')
    if PRIVATE_KEY:
        if not PRIVATE_KEY.startswith('${') or not PRIVATE_KEY.endswith('}'):
            raise ValueError("PRIVATE_KEY must reference an environment variable using ${VARIABLE_NAME} format. Never store private keys directly in files!")
        
        # Extract environment variable name and get its value
        env_var_name = PRIVATE_KEY[2:-1]
        PRIVATE_KEY = os.getenv(env_var_name)
        
        if not PRIVATE_KEY:
            raise ValueError(f"Environment variable '{env_var_name}' referenced in PRIVATE_KEY is not set")
    else:
        # PRIVATE_KEY not set - this is OK for testing/imports
        PRIVATE_KEY = None
    
    # Chain configuration (all from environment)
    CHAIN_ID = int(os.getenv('CHAIN_ID', '1'))
    CHAIN_NAME = os.getenv('CHAIN_NAME', 'Ethereum Mainnet')
    
    # Gas settings
    MAX_GAS_LIMIT = int(os.getenv('MAX_GAS_LIMIT', '500000'))
    
    # Token pair configuration
    TOKEN_A_ADDRESS = os.getenv('TOKEN_A_ADDRESS')
    TOKEN_B_ADDRESS = os.getenv('TOKEN_B_ADDRESS')
    
    # Fee tier configuration
    FEE_TIER = int(os.getenv('FEE_TIER', '5'))  # 0.05% fee tier (5 bps)
    
    # Model configuration
    INVENTORY_MODEL = os.getenv('INVENTORY_MODEL', 'AvellanedaStoikovModel')  # Model to use for range calculation
    
    # Uniswap V3 contract addresses (from environment)
    UNISWAP_V3_FACTORY = os.getenv('UNISWAP_V3_FACTORY')
    UNISWAP_V3_POSITION_MANAGER = os.getenv('UNISWAP_V3_POSITION_MANAGER')
    UNISWAP_V3_ROUTER = os.getenv('UNISWAP_V3_ROUTER')
    WETH_ADDRESS = os.getenv('WETH_ADDRESS')
    
    # Common token addresses (from environment)
    USDC_ADDRESS = os.getenv('USDC_ADDRESS')
    USDT_ADDRESS = os.getenv('USDT_ADDRESS')
    DAI_ADDRESS = os.getenv('DAI_ADDRESS')
    WBTC_ADDRESS = os.getenv('WBTC_ADDRESS')
    
    # Rebalancing configuration
    MIN_RANGE_PERCENTAGE = float(os.getenv('MIN_RANGE_PERCENTAGE', '2.0'))
    MAX_RANGE_PERCENTAGE = float(os.getenv('MAX_RANGE_PERCENTAGE', '50.0'))
    MONITORING_INTERVAL_SECONDS = int(os.getenv('MONITORING_INTERVAL_SECONDS', '60'))
    REBALANCE_THRESHOLD = float(os.getenv('REBALANCE_THRESHOLD', '0.10'))  # 10% deviation threshold
    
    # Backtesting-only parameters (only loaded when BACKTEST_MODE=true or .env.backtest exists)
    # These have defaults but are only meaningful in backtest mode
    TRADE_DETECTION_THRESHOLD = float(os.getenv('TRADE_DETECTION_THRESHOLD', '0.0005'))  # 0.05% threshold for trade detection (5 bps) - BACKTEST ONLY
    POSITION_FALLOFF_FACTOR = float(os.getenv('POSITION_FALLOFF_FACTOR', '0.1'))  # Minimum 10% value for position falloff - BACKTEST ONLY
    RISK_FREE_RATE = float(os.getenv('RISK_FREE_RATE', '0.02'))  # 2% annual risk-free rate - BACKTEST ONLY
    DEFAULT_DAILY_VOLATILITY = float(os.getenv('DEFAULT_DAILY_VOLATILITY', '0.02'))  # 2% daily volatility - BACKTEST ONLY
    DEFAULT_INITIAL_BALANCE_0 = float(os.getenv('DEFAULT_INITIAL_BALANCE_0', '2500.0'))  # Default initial token A balance - BACKTEST ONLY
    DEFAULT_INITIAL_BALANCE_1 = float(os.getenv('DEFAULT_INITIAL_BALANCE_1', '1.0'))  # Default initial token B balance - BACKTEST ONLY
    
    # Inventory management model parameters
    INVENTORY_RISK_AVERSION = float(os.getenv('INVENTORY_RISK_AVERSION', '0.1'))
    TARGET_INVENTORY_RATIO = float(os.getenv('TARGET_INVENTORY_RATIO', '0.5'))
    MAX_INVENTORY_DEVIATION = float(os.getenv('MAX_INVENTORY_DEVIATION', '0.3'))
    BASE_SPREAD = float(os.getenv('BASE_SPREAD', '0.20'))
    
    # Volatility calculation parameters
    VOLATILITY_WINDOW_SIZE = int(os.getenv('VOLATILITY_WINDOW_SIZE', '20'))
    DEFAULT_VOLATILITY = float(os.getenv('DEFAULT_VOLATILITY', '0.02'))
    MAX_VOLATILITY = float(os.getenv('MAX_VOLATILITY', '2.0'))
    MIN_VOLATILITY = float(os.getenv('MIN_VOLATILITY', '0.01'))
    
    # GLFT model specific parameters
    EXECUTION_COST = float(os.getenv('EXECUTION_COST', '0.001'))  # 0.1% execution cost
    INVENTORY_PENALTY = float(os.getenv('INVENTORY_PENALTY', '0.05'))  # Inventory holding penalty
    MAX_POSITION_SIZE = float(os.getenv('MAX_POSITION_SIZE', '0.1'))  # 10% max position size
    TERMINAL_INVENTORY_PENALTY = float(os.getenv('TERMINAL_INVENTORY_PENALTY', '0.2'))  # Terminal penalty
    INVENTORY_CONSTRAINT_ACTIVE = os.getenv('INVENTORY_CONSTRAINT_ACTIVE', 'false').lower() == 'true'
    
    # ZMQ publishing preferences
    ASSET_TOKEN = os.getenv('ASSET_TOKEN', '').strip()  # Which token to publish deltas for (e.g., 'A', 'B', 'ETH', 'USDC')
    
    # Telegram alerting
    TELEGRAM_BOT_TOKEN = os.getenv('TELEGRAM_BOT_TOKEN', '')
    TELEGRAM_CHAT_ID = os.getenv('TELEGRAM_CHAT_ID', '')
    TELEGRAM_ENABLED = bool(TELEGRAM_BOT_TOKEN and TELEGRAM_CHAT_ID)
    
    # Error handling
    MAX_RETRIES = int(os.getenv('MAX_RETRIES', '3'))
    RETRY_DELAY_SECONDS = float(os.getenv('RETRY_DELAY_SECONDS', '2.0'))
    
    # 0MQ inventory publishing
    ZMQ_ENABLED = os.getenv('ZMQ_ENABLED', 'false').lower() == 'true'
    ZMQ_PUBLISHER_PORT = int(os.getenv('ZMQ_PUBLISHER_PORT', '5555'))
    ZMQ_PUBLISHER_HOST = os.getenv('ZMQ_PUBLISHER_HOST', 'localhost')
    
    # Token decimals are now fetched dynamically from blockchain
    # No need for hardcoded values
    
    @classmethod
    def validate_config(cls) -> bool:
        """Validate that required configuration is present"""
        errors = []
        
        # Private key validation is handled in __init__, no need to check here
        
        if not cls.ETHEREUM_RPC_URL or 'YOUR_PROJECT_ID' in cls.ETHEREUM_RPC_URL:
            errors.append("ETHEREUM_RPC_URL must be set to a valid RPC endpoint")
        
        if not cls.TOKEN_A_ADDRESS:
            errors.append("TOKEN_A_ADDRESS is required")
        
        if not cls.TOKEN_B_ADDRESS:
            errors.append("TOKEN_B_ADDRESS is required")
        
        if not cls.UNISWAP_V3_FACTORY:
            errors.append("UNISWAP_V3_FACTORY is required")
        
        if not cls.UNISWAP_V3_POSITION_MANAGER:
            errors.append("UNISWAP_V3_POSITION_MANAGER is required")
        
        if not cls.UNISWAP_V3_ROUTER:
            errors.append("UNISWAP_V3_ROUTER is required")
        
        if not cls.WETH_ADDRESS:
            errors.append("WETH_ADDRESS is required")
        
        # Validate fee tier
        if cls.FEE_TIER not in [5, 500, 3000, 10000]:
            errors.append(f"FEE_TIER must be one of [5, 500, 3000, 10000], got {cls.FEE_TIER}")
        
        # Validate addresses format
        if cls.TOKEN_A_ADDRESS and not cls._is_valid_address(cls.TOKEN_A_ADDRESS):
            errors.append(f"Invalid TOKEN_A_ADDRESS format: {cls.TOKEN_A_ADDRESS}")
        
        if cls.TOKEN_B_ADDRESS and not cls._is_valid_address(cls.TOKEN_B_ADDRESS):
            errors.append(f"Invalid TOKEN_B_ADDRESS format: {cls.TOKEN_B_ADDRESS}")
        
        if cls.UNISWAP_V3_FACTORY and not cls._is_valid_address(cls.UNISWAP_V3_FACTORY):
            errors.append(f"Invalid UNISWAP_V3_FACTORY format: {cls.UNISWAP_V3_FACTORY}")
        
        if cls.UNISWAP_V3_POSITION_MANAGER and not cls._is_valid_address(cls.UNISWAP_V3_POSITION_MANAGER):
            errors.append(f"Invalid UNISWAP_V3_POSITION_MANAGER format: {cls.UNISWAP_V3_POSITION_MANAGER}")
        
        if cls.UNISWAP_V3_ROUTER and not cls._is_valid_address(cls.UNISWAP_V3_ROUTER):
            errors.append(f"Invalid UNISWAP_V3_ROUTER format: {cls.UNISWAP_V3_ROUTER}")
        
        if cls.WETH_ADDRESS and not cls._is_valid_address(cls.WETH_ADDRESS):
            errors.append(f"Invalid WETH_ADDRESS format: {cls.WETH_ADDRESS}")
        
        if errors:
            raise ValueError("Configuration validation failed:\n" + "\n".join(f"- {error}" for error in errors))
        
        return True
    
    @classmethod
    def validate_token_addresses(cls) -> bool:
        """
        Validate that token addresses are properly configured
        
        Returns:
            True if token addresses are valid
        """
        errors = []
        
        # Check token addresses format
        if not cls._is_valid_address(cls.TOKEN_A_ADDRESS):
            errors.append(f"Invalid TOKEN_A_ADDRESS format: {cls.TOKEN_A_ADDRESS}")
        
        if not cls._is_valid_address(cls.TOKEN_B_ADDRESS):
            errors.append(f"Invalid TOKEN_B_ADDRESS format: {cls.TOKEN_B_ADDRESS}")
        
        # Check that tokens are different
        if cls.TOKEN_A_ADDRESS and cls.TOKEN_B_ADDRESS and cls.TOKEN_A_ADDRESS.lower() == cls.TOKEN_B_ADDRESS.lower():
            errors.append("TOKEN_A_ADDRESS and TOKEN_B_ADDRESS cannot be the same")
        
        if errors:
            raise ValueError("Token address validation failed:\n" + "\n".join(f"- {error}" for error in errors))
        
        return True
    
    def detect_chain_from_rpc(self) -> str:
        """Detect chain name from RPC URL"""
        # Simple detection based on common RPC patterns
        rpc_url = self.ETHEREUM_RPC_URL.lower()
        
        if 'mainnet' in rpc_url or 'ethereum' in rpc_url:
            return 'ethereum'
        elif 'polygon' in rpc_url or 'matic' in rpc_url:
            return 'polygon'
        elif 'arbitrum' in rpc_url:
            return 'arbitrum'
        elif 'optimism' in rpc_url:
            return 'optimism'
        elif 'base' in rpc_url:
            return 'base'
        else:
            return 'ethereum'  # Default fallback
    
    def get_chain_config(self, chain_name: str) -> Dict[str, Any]:
        """Get chain configuration by name"""
        # Return basic chain info from environment variables
        return {
            'name': self.CHAIN_NAME,
            'chain_id': self.CHAIN_ID,
            'rpc_url': self.ETHEREUM_RPC_URL
        }
    
    def get_chain_info(self) -> Dict[str, Any]:
        """Get current chain information"""
        return {
            'chain_name': self.CHAIN_NAME,
            'chain_id': self.CHAIN_ID,
            'rpc_url': self.ETHEREUM_RPC_URL
        }
    
    @staticmethod
    def _is_valid_address(address: str) -> bool:
        """Check if address is valid Ethereum address format"""
        if not address:
            return False
        
        # Check basic format
        if not (address.startswith('0x') and len(address) == 42):
            return False
        
        # Check that all characters after 0x are valid hex
        if not all(c in '0123456789abcdefABCDEF' for c in address[2:]):
            return False
        
        # Try to checksum the address to validate it's properly formatted
        try:
            from web3 import Web3
            checksummed = Web3.to_checksum_address(address)
            # Verify checksumming didn't fail (would raise exception)
            return checksummed is not None
        except (ValueError, TypeError) as e:
            # Log the specific error for debugging
            import logging
            logging.getLogger(__name__).debug(f"Address validation failed for {address}: {e}")
            return False
        except Exception as e:
            # Catch any other exceptions (e.g., import errors)
            import logging
            logging.getLogger(__name__).warning(f"Unexpected error validating address {address}: {e}")
            return False
    
    @classmethod
    def get_chain_info(cls) -> Dict[str, Any]:
        """Get chain information from environment"""
        return {
            'chain_id': cls.CHAIN_ID,
            'chain_name': cls.CHAIN_NAME,
            'factory': cls.UNISWAP_V3_FACTORY,
            'position_manager': cls.UNISWAP_V3_POSITION_MANAGER,
            'router': cls.UNISWAP_V3_ROUTER,
            'weth': cls.WETH_ADDRESS
        }
