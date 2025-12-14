"""
Unit tests for AsymmetricLP strategy logic.
Tests the core business logic without external dependencies.
"""
import pytest
import sys
import os
from unittest.mock import Mock, patch

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from strategy import AsymmetricLPStrategy
from models.avellaneda_stoikov import AvellanedaStoikovModel
from models.glft_model import GLFTModel
from config import Config


class TestAsymmetricLPStrategy:
    """Test the core strategy logic."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.REBALANCE_THRESHOLD = 0.4  # 40%
        self.config.BASE_SPREAD = 0.15  # 15%
        self.config.MIN_RANGE_WIDTH = 0.01  # 1%
        self.config.MAX_RANGE_WIDTH = 0.5  # 50%
        
        self.inventory_model = Mock()
        self.strategy = AsymmetricLPStrategy(
            config=self.config,
            inventory_model=self.inventory_model
        )
    
    def test_should_rebalance_no_deviation(self):
        """Test that no rebalance occurs when tokens are not depleted."""
        current_price = 3000.0
        price_history = []
        last_rebalance_token0 = 1000.0
        last_rebalance_token1 = 0.5
        last_rebalance_price = 3000.0
        current_token0 = 1000.0
        current_token1 = 0.5
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            current_token0=current_token0,
            current_token1=current_token1,
            last_rebalance_token0=last_rebalance_token0,
            last_rebalance_token1=last_rebalance_token1,
            last_rebalance_price=last_rebalance_price,
            has_positions=True
        )
        
        assert result is False
    
    def test_should_rebalance_token0_depleted(self):
        """Test that rebalance occurs when token0 is depleted beyond threshold."""
        current_price = 3000.0
        price_history = []
        last_rebalance_token0 = 1000.0
        last_rebalance_token1 = 0.5
        last_rebalance_price = 3000.0
        current_token0 = 500.0  # 50% depleted (above 40% threshold)
        current_token1 = 0.5
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            current_token0=current_token0,
            current_token1=current_token1,
            last_rebalance_token0=last_rebalance_token0,
            last_rebalance_token1=last_rebalance_token1,
            last_rebalance_price=last_rebalance_price,
            has_positions=True
        )
        
        assert result is True
    
    def test_should_rebalance_token1_depleted(self):
        """Test that rebalance occurs when token1 is depleted beyond threshold."""
        current_price = 3000.0
        price_history = []
        last_rebalance_token0 = 1000.0
        last_rebalance_token1 = 0.5
        last_rebalance_price = 3000.0
        current_token0 = 1000.0
        current_token1 = 0.2  # 60% depleted (above 40% threshold)
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            current_token0=current_token0,
            current_token1=current_token1,
            last_rebalance_token0=last_rebalance_token0,
            last_rebalance_token1=last_rebalance_token1,
            last_rebalance_price=last_rebalance_price,
            has_positions=True
        )
        
        assert result is True
    
    def test_should_rebalance_both_tokens_depleted(self):
        """Test that rebalance occurs when both tokens are depleted."""
        current_price = 3000.0
        price_history = []
        last_rebalance_token0 = 1000.0
        last_rebalance_token1 = 0.5
        last_rebalance_price = 3000.0
        current_token0 = 500.0  # 50% depleted (above 40% threshold)
        current_token1 = 0.2  # 60% depleted (above 40% threshold)
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            current_token0=current_token0,
            current_token1=current_token1,
            last_rebalance_token0=last_rebalance_token0,
            last_rebalance_token1=last_rebalance_token1,
            last_rebalance_price=last_rebalance_price,
            has_positions=True
        )
        
        assert result is True
    
    def test_should_rebalance_edge_case_zero_balances(self):
        """Test edge case with zero initial balances."""
        current_price = 3000.0
        price_history = []
        last_rebalance_token0 = 0.0
        last_rebalance_token1 = 0.0
        last_rebalance_price = 3000.0
        current_token0 = 0.0
        current_token1 = 0.0
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            current_token0=current_token0,
            current_token1=current_token1,
            last_rebalance_token0=last_rebalance_token0,
            last_rebalance_token1=last_rebalance_token1,
            last_rebalance_price=last_rebalance_price,
            has_positions=False  # No positions means first mint should be allowed
        )
        
        assert result is True  # Should allow first mint when has_positions=False
    
    def test_plan_rebalance_basic(self):
        """Test basic rebalance planning."""
        from unittest.mock import Mock
        
        current_price = 3000.0
        price_history = []
        token0_balance = 1000.0
        token1_balance = 0.5
        initial_target_ratio = 0.5
        startup_allocation = False
        
        # Mock the inventory model and client
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        self.inventory_model.calculate_lp_ranges.return_value = {
            'range_a_percentage': 10.0,
            'range_b_percentage': 10.0,
            'inventory_ratio': 0.5,
            'target_ratio': 0.5,
            'deviation': 0.0,
            'volatility': 0.02
        }
        
        result = self.strategy.plan_rebalance(
            current_price=current_price,
            price_history=price_history,
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            initial_target_ratio=initial_target_ratio,
            startup_allocation=startup_allocation,
            client=mock_client,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB'
        )
        
        assert result is not None
        assert len(result) == 5  # Returns tuple of 5 values
        range_a_pct, range_b_pct, adj_token0, adj_token1, ranges_raw = result
        assert isinstance(range_a_pct, float)
        assert isinstance(range_b_pct, float)
    
    def test_plan_rebalance_with_price_history(self):
        """Test rebalance planning with price history."""
        from unittest.mock import Mock
        
        current_price = 3000.0
        price_history = [
            {'price': 2900.0, 'timestamp': 1000},
            {'price': 2950.0, 'timestamp': 2000},
            {'price': 3000.0, 'timestamp': 3000}
        ]
        token0_balance = 1000.0
        token1_balance = 0.5
        initial_target_ratio = 0.5
        startup_allocation = False
        
        # Mock the inventory model and client
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        self.inventory_model.calculate_lp_ranges.return_value = {
            'range_a_percentage': 10.0,
            'range_b_percentage': 10.0,
            'inventory_ratio': 0.5,
            'target_ratio': 0.5,
            'deviation': 0.0,
            'volatility': 0.02
        }
        
        result = self.strategy.plan_rebalance(
            current_price=current_price,
            price_history=price_history,
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            initial_target_ratio=initial_target_ratio,
            startup_allocation=startup_allocation,
            client=mock_client,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB'
        )
        
        assert result is not None
        assert len(result) == 5  # Returns tuple of 5 values


class TestAvellanedaStoikovModel:
    """Test the Avellaneda-Stoikov model."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.VOLATILITY = 0.3
        self.config.RISK_AVERSION = 0.1
        self.config.INVENTORY_PENALTY = 0.01
        # Add all required config attributes
        self.config.INVENTORY_RISK_AVERSION = 0.1
        self.config.TARGET_INVENTORY_RATIO = 0.5
        self.config.MAX_INVENTORY_DEVIATION = 0.3
        self.config.BASE_SPREAD = 0.05
        self.config.VOLATILITY_WINDOW_SIZE = 20
        self.config.DEFAULT_VOLATILITY = 0.02
        self.config.MAX_VOLATILITY = 2.0
        self.config.MIN_VOLATILITY = 0.01
        self.config.MIN_RANGE_PERCENTAGE = 1.0  # 1%
        self.config.MAX_RANGE_PERCENTAGE = 50.0  # 50%
        
        self.model = AvellanedaStoikovModel(self.config)
    
    def test_calculate_lp_ranges_basic(self):
        """Test basic LP range calculation."""
        from unittest.mock import Mock
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        token0_balance = 1000 * 10**18
        token1_balance = 1 * 10**18
        spot_price = 3000.0
        price_history = []
        
        result = self.model.calculate_lp_ranges(
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        assert isinstance(result, dict)
        assert 'range_a_percentage' in result
        assert 'range_b_percentage' in result
        assert result['range_a_percentage'] > 0
        assert result['range_b_percentage'] > 0
    
    def test_calculate_lp_ranges_edge_cases(self):
        """Test edge cases for LP range calculation."""
        from unittest.mock import Mock
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        spot_price = 3000.0
        price_history = []
        
        # Test with extreme inventory ratios
        result_min = self.model.calculate_lp_ranges(
            token0_balance=1 * 10**18,
            token1_balance=1000 * 10**18,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        result_max = self.model.calculate_lp_ranges(
            token0_balance=1000 * 10**18,
            token1_balance=1 * 10**18,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        assert isinstance(result_min, dict)
        assert isinstance(result_max, dict)
        assert 'range_a_percentage' in result_min
        assert 'range_b_percentage' in result_max


class TestGLFTModel:
    """Test the GLFT model."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        # Add all required config attributes
        self.config.INVENTORY_RISK_AVERSION = 0.1
        self.config.TARGET_INVENTORY_RATIO = 0.5
        self.config.MAX_INVENTORY_DEVIATION = 0.3
        self.config.BASE_SPREAD = 0.05
        self.config.VOLATILITY_WINDOW_SIZE = 20
        self.config.DEFAULT_VOLATILITY = 0.02
        self.config.MAX_VOLATILITY = 2.0
        self.config.MIN_VOLATILITY = 0.01
        self.config.MAX_POSITION_SIZE = 0.1
        self.config.TERMINAL_INVENTORY_PENALTY = 0.2
        self.config.INVENTORY_CONSTRAINT_ACTIVE = False
        self.config.MIN_RANGE_PERCENTAGE = 1.0  # 1%
        self.config.MAX_RANGE_PERCENTAGE = 50.0  # 50%
        
        self.model = GLFTModel(self.config)
    
    def test_calculate_lp_ranges_basic(self):
        """Test basic LP range calculation."""
        from unittest.mock import Mock
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        token0_balance = 1000 * 10**18
        token1_balance = 1 * 10**18
        spot_price = 3000.0
        price_history = []
        
        result = self.model.calculate_lp_ranges(
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        assert isinstance(result, dict)
        assert 'range_a_percentage' in result
        assert 'range_b_percentage' in result
        assert result['range_a_percentage'] > 0
        assert result['range_b_percentage'] > 0
    
    def test_calculate_lp_ranges_with_execution_cost(self):
        """Test LP range calculation with execution cost."""
        from unittest.mock import Mock
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        token0_balance = 1000 * 10**18
        token1_balance = 1 * 10**18
        spot_price = 3000.0
        price_history = []
        
        result = self.model.calculate_lp_ranges(
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        assert isinstance(result, dict)
        assert 'range_a_percentage' in result
        assert result['range_a_percentage'] > 0


if __name__ == "__main__":
    pytest.main([__file__])
