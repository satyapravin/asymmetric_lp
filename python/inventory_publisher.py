"""
AsymmetricLP - Inventory Publisher (Stub)

This module previously published inventory data to C++ via ZMQ.
The ZMQ connectivity has been removed - Python and C++ now operate independently.

This stub class is kept for backward compatibility with automated_rebalancer.py.
All methods are no-ops.
"""
import logging
from typing import Dict, Any
from config import Config

logger = logging.getLogger(__name__)


class InventoryPublisher:
    """
    Stub inventory publisher - all methods are no-ops.
    
    Previously published inventory data via ZMQ for CeFi MM integration.
    Now Python DeFi LP and C++ CeFi MM operate independently.
    """
    
    def __init__(self, config: Config):
        """
        Initialize stub publisher
        
        Args:
            config: Configuration object (not used)
        """
        self.config = config
        logger.info("InventoryPublisher initialized (stub - no ZMQ publishing)")
    
    def update_inventory_data(self, 
                            token_a_address: str,
                            token_b_address: str,
                            token_a_balance: float,
                            token_b_balance: float) -> None:
        """
        Stub method - does nothing
        
        Args:
            token_a_address: Token A address
            token_b_address: Token B address
            token_a_balance: Token A balance
            token_b_balance: Token B balance
        """
        pass
    
    def publish_rebalance_event(self,
                              token_a_symbol: str,
                              token_b_symbol: str,
                              spot_price: float,
                              old_ratio: float,
                              new_ratio: float,
                              fees_collected: Dict[str, float],
                              gas_used: int,
                              ranges: Dict[str, float]):
        """
        Stub method - does nothing
        """
        pass
    
    def publish_error_event(self,
                           error_type: str,
                           error_message: str,
                           context: Dict[str, Any]):
        """
        Stub method - does nothing
        """
        pass
    
    def get_publisher_stats(self) -> Dict[str, Any]:
        """
        Get publisher statistics
        
        Returns:
            Dictionary with stub stats
        """
        return {
            'enabled': False,
            'status': 'stub - ZMQ removed'
        }
