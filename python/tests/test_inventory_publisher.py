"""Unit tests for InventoryPublisher stub."""
import pytest
import sys
import os
from unittest.mock import Mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from inventory_publisher import InventoryPublisher


class TestInventoryPublisher:
    """Test the inventory publisher stub."""
    
    def test_initialization(self):
        """Test publisher initialization."""
        config = Mock()
        publisher = InventoryPublisher(config)
        assert publisher.config == config
    
    def test_update_inventory_data_noop(self):
        """Test update_inventory_data is a no-op."""
        publisher = InventoryPublisher(Mock())
        publisher.update_inventory_data('0xA', '0xB', 1.0, 0.5)
    
    def test_publish_rebalance_event_noop(self):
        """Test publish_rebalance_event is a no-op."""
        publisher = InventoryPublisher(Mock())
        publisher.publish_rebalance_event('ETH', 'USDC', 2000.0, 0.5, 0.6, {}, 100000, {})
    
    def test_publish_error_event_noop(self):
        """Test publish_error_event is a no-op."""
        publisher = InventoryPublisher(Mock())
        publisher.publish_error_event('Error', 'msg', {})
    
    def test_get_publisher_stats(self):
        """Test get_publisher_stats returns stub info."""
        publisher = InventoryPublisher(Mock())
        stats = publisher.get_publisher_stats()
        assert stats['enabled'] == False
        assert 'stub' in stats['status'].lower()

