"""
Configuration management for AsymmetricLP.
Loads settings from environment variables.
"""
import os
from typing import Dict, Any
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

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
    FEE_TIER = int(os.getenv('FEE_TIER', '30'))  # 0.3% fee tier (30 bps)
    
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
    MAX_RANGE_PERCENTAGE = float(os.getenv('MAX_RANGE_PERCENTAGE', '20.0'))
    MONITORING_INTERVAL_SECONDS = int(os.getenv('MONITORING_INTERVAL_SECONDS', '60'))
    
    # Inventory management model parameters
    INVENTORY_RISK_AVERSION = float(os.getenv('INVENTORY_RISK_AVERSION', '0.1'))
    TARGET_INVENTORY_RATIO = float(os.getenv('TARGET_INVENTORY_RATIO', '0.5'))
    MAX_INVENTORY_DEVIATION = float(os.getenv('MAX_INVENTORY_DEVIATION', '0.3'))
    BASE_SPREAD = float(os.getenv('BASE_SPREAD', '0.01'))
    
    # Volatility calculation parameters
    VOLATILITY_WINDOW_SIZE = int(os.getenv('VOLATILITY_WINDOW_SIZE', '20'))
    DEFAULT_VOLATILITY = float(os.getenv('DEFAULT_VOLATILITY', '0.02'))
    MAX_VOLATILITY = float(os.getenv('MAX_VOLATILITY', '2.0'))
    MIN_VOLATILITY = float(os.getenv('MIN_VOLATILITY', '0.01'))
    
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
        if cls.FEE_TIER not in [500, 3000, 10000]:
            errors.append(f"FEE_TIER must be one of [500, 3000, 10000], got {cls.FEE_TIER}")
        
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
        return address.startswith('0x') and len(address) == 42 and all(c in '0123456789abcdefABCDEF' for c in address[2:])
    
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
