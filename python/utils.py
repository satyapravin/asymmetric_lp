"""
AsymmetricLP - Utility Functions
Utility functions for Uniswap V3 operations and error handling
"""
import logging
import math
import time
import functools
from typing import Dict, Any, Optional, Tuple, Callable
from web3 import Web3
from web3.exceptions import ContractLogicError, TransactionNotFound, Web3Exception

logger = logging.getLogger(__name__)

class UniswapV3Utils:
    """Utility functions for Uniswap V3 operations"""
    
    @staticmethod
    def tick_to_price(tick: int) -> float:
        """
        Convert tick to price
        
        Args:
            tick: Tick value
            
        Returns:
            Price as float
        """
        return math.sqrt(1.0001 ** tick)
    
    @staticmethod
    def price_to_tick(price: float) -> int:
        """
        Convert price to tick
        
        Args:
            price: Price as float
            
        Returns:
            Tick value
        """
        return int(math.log(price ** 2) / math.log(1.0001))
    
    @staticmethod
    def calculate_tick_spacing(fee: int) -> int:
        """
        Calculate tick spacing for a given fee tier
        
        Args:
            fee: Fee in basis points (500, 3000, 10000)
            
        Returns:
            Tick spacing
        """
        if fee == 100:  # 0.01%
            return 1
        elif fee == 500:  # 0.05%
            return 10
        elif fee == 3000:  # 0.3%
            return 60
        elif fee == 10000:  # 1%
            return 200
        else:
            raise ValueError(f"Unsupported fee tier: {fee}")
    
    @staticmethod
    def get_tick_at_price(price: float, tick_spacing: int) -> int:
        """
        Get the nearest tick for a given price
        
        Args:
            price: Target price
            tick_spacing: Tick spacing
            
        Returns:
            Nearest tick
        """
        tick = UniswapV3Utils.price_to_tick(price)
        return (tick // tick_spacing) * tick_spacing
    
    @staticmethod
    def calculate_liquidity_amounts(
        amount0: int,
        amount1: int,
        tick_lower: int,
        tick_upper: int,
        current_tick: int,
        token0_decimals: int = 18,
        token1_decimals: int = 18
    ) -> Tuple[int, int]:
        """
        Calculate actual amounts needed for liquidity position
        
        Args:
            amount0: Desired amount of token0
            amount1: Desired amount of token1
            tick_lower: Lower tick bound
            tick_upper: Upper tick bound
            current_tick: Current pool tick
            token0_decimals: Token0 decimals
            token1_decimals: Token1 decimals
            
        Returns:
            Tuple of (actual_amount0, actual_amount1)
        """
        # This is a simplified calculation
        # In practice, you'd need to implement the full Uniswap V3 math
        
        if current_tick < tick_lower:
            # Only token0 needed
            return amount0, 0
        elif current_tick > tick_upper:
            # Only token1 needed
            return 0, amount1
        else:
            # Both tokens needed (simplified)
            return amount0, amount1
    
    @staticmethod
    def format_token_amount(amount: int, decimals: int, symbol: str = "") -> str:
        """
        Format token amount for display
        
        Args:
            amount: Amount in wei
            decimals: Token decimals
            symbol: Token symbol
            
        Returns:
            Formatted string
        """
        formatted_amount = amount / (10 ** decimals)
        return f"{formatted_amount:.6f} {symbol}".strip()
    
    @staticmethod
    def parse_token_amount(amount_str: str, decimals: int) -> int:
        """
        Parse token amount string to wei
        
        Args:
            amount_str: Amount string (e.g., "1000.5")
            decimals: Token decimals
            
        Returns:
            Amount in wei
        """
        try:
            amount = float(amount_str)
            return int(amount * (10 ** decimals))
        except ValueError:
            raise ValueError(f"Invalid amount format: {amount_str}")

class ErrorHandler:
    """Error handling utilities"""
    
    @staticmethod
    def handle_transaction_error(error: Exception) -> Dict[str, Any]:
        """
        Handle transaction errors and provide meaningful messages
        
        Args:
            error: Exception object
            
        Returns:
            Error information dictionary
        """
        error_msg = str(error)
        
        if "insufficient funds" in error_msg.lower():
            return {
                'type': 'insufficient_funds',
                'message': 'Insufficient ETH balance for gas fees',
                'suggestion': 'Add more ETH to your wallet'
            }
        elif "gas limit" in error_msg.lower():
            return {
                'type': 'gas_limit',
                'message': 'Transaction gas limit exceeded',
                'suggestion': 'Increase gas limit or optimize transaction'
            }
        elif "slippage" in error_msg.lower():
            return {
                'type': 'slippage',
                'message': 'Price slippage too high',
                'suggestion': 'Increase slippage tolerance or reduce position size'
            }
        elif "deadline" in error_msg.lower():
            return {
                'type': 'deadline',
                'message': 'Transaction deadline exceeded',
                'suggestion': 'Increase deadline or retry transaction'
            }
        elif "nonce" in error_msg.lower():
            return {
                'type': 'nonce',
                'message': 'Nonce error',
                'suggestion': 'Wait for pending transactions or reset nonce'
            }
        else:
            return {
                'type': 'unknown',
                'message': error_msg,
                'suggestion': 'Check transaction parameters and try again'
            }
    
    @staticmethod
    def validate_transaction_params(params: Dict[str, Any]) -> bool:
        """
        Validate transaction parameters
        
        Args:
            params: Transaction parameters
            
        Returns:
            True if valid, raises exception if invalid
        """
        required_fields = ['token0', 'token1', 'fee', 'amount0_desired', 'amount1_desired']
        
        for field in required_fields:
            if field not in params:
                raise ValueError(f"Missing required parameter: {field}")
        
        # Validate addresses
        if not Web3.is_address(params['token0']):
            raise ValueError(f"Invalid token0 address: {params['token0']}")
        
        if not Web3.is_address(params['token1']):
            raise ValueError(f"Invalid token1 address: {params['token1']}")
        
        # Validate amounts
        if params['amount0_desired'] <= 0:
            raise ValueError("amount0_desired must be positive")
        
        if params['amount1_desired'] <= 0:
            raise ValueError("amount1_desired must be positive")
        
        # Validate fee
        valid_fees = [100, 500, 3000, 10000]
        if params['fee'] not in valid_fees:
            raise ValueError(f"Invalid fee tier: {params['fee']}. Valid fees: {valid_fees}")
        
        return True

class Logger:
    """Enhanced logging utilities"""
    
    @staticmethod
    def setup_logging(level: str = "INFO", log_file: str = None):
        """
        Set up logging configuration
        
        Args:
            level: Logging level (DEBUG, INFO, WARNING, ERROR)
            log_file: Optional log file path
        """
        log_level = getattr(logging, level.upper())
        
        # Create formatter
        formatter = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        
        # Set up console handler
        console_handler = logging.StreamHandler()
        console_handler.setFormatter(formatter)
        
        # Set up file handler if specified
        handlers = [console_handler]
        if log_file:
            file_handler = logging.FileHandler(log_file)
            file_handler.setFormatter(formatter)
            handlers.append(file_handler)
        
        # Configure root logger
        logging.basicConfig(
            level=log_level,
            handlers=handlers
        )
    
    @staticmethod
    def log_transaction(tx_hash: str, operation: str, success: bool, details: Dict[str, Any] = None):
        """
        Log transaction details
        
        Args:
            tx_hash: Transaction hash
            operation: Operation type (add_liquidity, remove_liquidity, etc.)
            success: Whether transaction was successful
            details: Additional details
        """
        status = "SUCCESS" if success else "FAILED"
        logger.info(f"Transaction {status}: {operation} - {tx_hash}")
        
        if details:
            for key, value in details.items():
                logger.info(f"  {key}: {value}")
    
    @staticmethod
    def log_position_info(position_info: Dict[str, Any]):
        """
        Log position information
        
        Args:
            position_info: Position information dictionary
        """
        logger.info("Position Information:")
        logger.info(f"  Token ID: {position_info.get('token_id', 'N/A')}")
        logger.info(f"  Token0: {position_info.get('token0', 'N/A')}")
        logger.info(f"  Token1: {position_info.get('token1', 'N/A')}")
        logger.info(f"  Fee: {position_info.get('fee', 'N/A')}")
        logger.info(f"  Tick Range: {position_info.get('tick_range', 'N/A')}")
        logger.info(f"  Current Tick: {position_info.get('current_tick', 'N/A')}")
        logger.info(f"  Liquidity: {position_info.get('liquidity', 'N/A')}")


def retry_on_failure(max_retries: int = 3, delay: float = 2.0, backoff_factor: float = 1.5):
    """
    Decorator to retry function calls on failure with exponential backoff
    
    Args:
        max_retries: Maximum number of retry attempts
        delay: Initial delay between retries in seconds
        backoff_factor: Multiplier for delay after each retry
    """
    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            current_delay = delay
            last_exception = None
            
            for attempt in range(max_retries + 1):
                try:
                    return func(*args, **kwargs)
                except (ContractLogicError, TransactionNotFound, Web3Exception, Exception) as e:
                    last_exception = e
                    
                    if attempt == max_retries:
                        logger.error(f"Function {func.__name__} failed after {max_retries} retries. Last error: {e}")
                        raise e
                    
                    logger.warning(f"Function {func.__name__} failed (attempt {attempt + 1}/{max_retries + 1}): {e}")
                    logger.info(f"Retrying in {current_delay:.1f} seconds...")
                    
                    time.sleep(current_delay)
                    current_delay *= backoff_factor
            
            # This should never be reached, but just in case
            raise last_exception
        
        return wrapper
    return decorator


def is_retryable_error(error: Exception) -> bool:
    """
    Check if an error is retryable
    
    Args:
        error: Exception to check
        
    Returns:
        True if error is retryable, False otherwise
    """
    retryable_errors = (
        ContractLogicError,
        TransactionNotFound,
        Web3Exception,
        ConnectionError,
        TimeoutError,
        OSError,
    )
    
    # Check for specific error messages that indicate retryable conditions
    error_str = str(error).lower()
    retryable_messages = [
        'connection',
        'timeout',
        'network',
        'gas',
        'nonce',
        'revert',
        'insufficient funds',
        'transaction failed',
        'execution reverted',
    ]
    
    return isinstance(error, retryable_errors) or any(msg in error_str for msg in retryable_messages)
