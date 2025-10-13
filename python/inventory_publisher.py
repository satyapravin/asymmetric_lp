"""
AsymmetricLP - 0MQ Inventory Publisher
Publishes inventory data to CeFi MM agents for unified inventory management
"""
import logging
import json
from typing import Dict, Any, Optional
from datetime import datetime
import zmq
from config import Config

logger = logging.getLogger(__name__)

class InventoryPublisher:
    """Publishes inventory data via 0MQ for CeFi MM integration"""
    
    def __init__(self, config: Config):
        """
        Initialize 0MQ inventory publisher
        
        Args:
            config: Configuration object
        """
        self.config = config
        self.enabled = config.ZMQ_ENABLED
        self.host = config.ZMQ_PUBLISHER_HOST
        self.port = config.ZMQ_PUBLISHER_PORT
        
        self.context = None
        self.socket = None
        self.last_inventory_data = None
        
        if self.enabled:
            self._initialize_zmq()
            logger.info(f"0MQ inventory publisher initialized on {self.host}:{self.port}")
        else:
            logger.info("0MQ inventory publisher disabled")
    
    def _initialize_zmq(self):
        """Initialize 0MQ context and socket"""
        try:
            self.context = zmq.Context()
            self.socket = self.context.socket(zmq.PUB)
            
            # Set socket options for reliability
            self.socket.setsockopt(zmq.LINGER, 1000)  # Wait 1 second for messages to be sent
            self.socket.setsockopt(zmq.SNDHWM, 1000)  # High water mark for outgoing messages
            
            # Bind to publisher address
            publisher_address = f"tcp://*:{self.port}"
            self.socket.bind(publisher_address)
            
            logger.info(f"0MQ publisher bound to {publisher_address}")
            
        except Exception as e:
            logger.error(f"Failed to initialize 0MQ publisher: {e}")
            self.enabled = False
    
    
    def update_inventory_data(self, 
                            token_a_symbol: str,
                            token_b_symbol: str,
                            token_a_address: str,
                            token_b_address: str,
                            spot_price: float,
                            inventory_ratio: float,
                            token_a_balance: float,
                            token_b_balance: float,
                            token_a_usd_value: float,
                            token_b_usd_value: float,
                            total_usd_value: float,
                            target_ratio: float,
                            deviation: float,
                            should_rebalance: bool,
                            volatility: float,
                            ranges: Dict[str, float]) -> None:
        """
        Update inventory data for publishing
        
        Args:
            token_a_symbol: Token A symbol
            token_b_symbol: Token B symbol
            token_a_address: Token A address
            token_b_address: Token B address
            spot_price: Current spot price
            inventory_ratio: Current inventory ratio
            token_a_balance: Token A balance
            token_b_balance: Token B balance
            token_a_usd_value: Token A USD value
            token_b_usd_value: Token B USD value
            total_usd_value: Total portfolio USD value
            target_ratio: Target inventory ratio
            deviation: Current deviation from target
            should_rebalance: Whether rebalancing is needed
            volatility: Current volatility
            ranges: Calculated ranges for both tokens
        """
        if not self.enabled:
            return
        
        # Create comprehensive inventory data
        inventory_data = {
            'timestamp': datetime.utcnow().isoformat(),
            'source': 'uniswap_v3_lp_rebalancer',
            'chain': self.config.CHAIN_NAME,
            'chain_id': self.config.CHAIN_ID,
            
            # Token information
            'tokens': {
                'token_a': {
                    'symbol': token_a_symbol,
                    'address': token_a_address,
                    'balance': token_a_balance,
                    'usd_value': token_a_usd_value,
                    'range_percentage': ranges.get('token_a_range', 0)
                },
                'token_b': {
                    'symbol': token_b_symbol,
                    'address': token_b_address,
                    'balance': token_b_balance,
                    'usd_value': token_b_usd_value,
                    'range_percentage': ranges.get('token_b_range', 0)
                }
            },
            
            # Market data
            'market': {
                'spot_price': spot_price,
                'volatility': volatility,
                'pair': f"{token_a_symbol}/{token_b_symbol}"
            },
            
            # Inventory metrics
            'inventory': {
                'current_ratio': inventory_ratio,
                'target_ratio': target_ratio,
                'deviation': deviation,
                'total_usd_value': total_usd_value,
                'should_rebalance': should_rebalance
            },
            
            # Notionals for CeFi MM
            'notionals': {
                'token_a_notional': token_a_usd_value,
                'token_b_notional': token_b_usd_value,
                'total_notional': total_usd_value,
                'imbalance_usd': abs(token_a_usd_value - token_b_usd_value),
                'imbalance_percentage': abs(inventory_ratio - target_ratio) * 100
            },
            
            # Configuration context
            'config': {
                'max_deviation': self.config.MAX_INVENTORY_DEVIATION,
                'risk_aversion': self.config.INVENTORY_RISK_AVERSION,
                'base_spread': self.config.BASE_SPREAD,
                'min_range': self.config.MIN_RANGE_PERCENTAGE,
                'max_range': self.config.MAX_RANGE_PERCENTAGE
            }
        }
        
        self.last_inventory_data = inventory_data
        
        # Publish inventory data immediately on every update
        self._publish_inventory_data(inventory_data)
    
    def _publish_inventory_data(self, inventory_data: Dict[str, Any]):
        """
        Publish inventory data via 0MQ
        
        Args:
            inventory_data: Inventory data dictionary
        """
        if not self.enabled or not self.socket:
            return
        
        try:
            # Create message with topic and data
            topic = "inventory_update"
            message = json.dumps(inventory_data, indent=None)
            
            # Send message with topic prefix
            full_message = f"{topic} {message}"
            self.socket.send_string(full_message, zmq.NOBLOCK)
            
            logger.debug(f"Published inventory data: {inventory_data['tokens']['token_a']['symbol']}/{inventory_data['tokens']['token_b']['symbol']} "
                        f"ratio={inventory_data['inventory']['current_ratio']:.3f} "
                        f"deviation={inventory_data['inventory']['deviation']:.3f}")
            
        except zmq.Again:
            logger.warning("0MQ send buffer full - message dropped")
        except Exception as e:
            logger.error(f"Failed to publish inventory data: {e}")
    
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
        Publish rebalance event data
        
        Args:
            token_a_symbol: Token A symbol
            token_b_symbol: Token B symbol
            spot_price: Current spot price
            old_ratio: Inventory ratio before rebalance
            new_ratio: Inventory ratio after rebalance
            fees_collected: Fees collected from burned positions
            gas_used: Gas used for rebalance
            ranges: New ranges for both tokens
        """
        if not self.enabled:
            return
        
        rebalance_data = {
            'timestamp': datetime.utcnow().isoformat(),
            'event_type': 'rebalance_completed',
            'source': 'uniswap_v3_lp_rebalancer',
            'chain': self.config.CHAIN_NAME,
            'chain_id': self.config.CHAIN_ID,
            
            'tokens': {
                'token_a_symbol': token_a_symbol,
                'token_b_symbol': token_b_symbol,
                'pair': f"{token_a_symbol}/{token_b_symbol}"
            },
            
            'rebalance': {
                'spot_price': spot_price,
                'old_inventory_ratio': old_ratio,
                'new_inventory_ratio': new_ratio,
                'ratio_change': new_ratio - old_ratio,
                'gas_used': gas_used
            },
            
            'fees': fees_collected,
            'ranges': ranges
        }
        
        try:
            topic = "rebalance_event"
            message = json.dumps(rebalance_data, indent=None)
            full_message = f"{topic} {message}"
            self.socket.send_string(full_message, zmq.NOBLOCK)
            
            logger.info(f"Published rebalance event: {token_a_symbol}/{token_b_symbol}")
            
        except Exception as e:
            logger.error(f"Failed to publish rebalance event: {e}")
    
    def publish_error_event(self,
                           error_type: str,
                           error_message: str,
                           context: Dict[str, Any]):
        """
        Publish error event data
        
        Args:
            error_type: Type of error
            error_message: Error message
            context: Additional context
        """
        if not self.enabled:
            return
        
        error_data = {
            'timestamp': datetime.utcnow().isoformat(),
            'event_type': 'error',
            'source': 'uniswap_v3_lp_rebalancer',
            'chain': self.config.CHAIN_NAME,
            'chain_id': self.config.CHAIN_ID,
            
            'error': {
                'type': error_type,
                'message': error_message,
                'context': context
            }
        }
        
        try:
            topic = "error_event"
            message = json.dumps(error_data, indent=None)
            full_message = f"{topic} {message}"
            self.socket.send_string(full_message, zmq.NOBLOCK)
            
            logger.info(f"Published error event: {error_type}")
            
        except Exception as e:
            logger.error(f"Failed to publish error event: {e}")
    
    def get_publisher_stats(self) -> Dict[str, Any]:
        """
        Get publisher statistics
        
        Returns:
            Dictionary with publisher stats
        """
        stats = {
            'enabled': self.enabled,
            'host': self.host,
            'port': self.port,
            'last_data_timestamp': self.last_inventory_data.get('timestamp') if self.last_inventory_data else None
        }
        
        if self.socket:
            try:
                stats['socket_connected'] = True
                # Get socket statistics if available
                stats['messages_sent'] = self.socket.getsockopt(zmq.SNDMORE) if hasattr(zmq, 'SNDMORE') else 'N/A'
            except:
                stats['socket_connected'] = False
        
        return stats
    
    def __del__(self):
        """Cleanup on destruction"""
        if self.socket:
            self.socket.close()
        if self.context:
            self.context.term()
