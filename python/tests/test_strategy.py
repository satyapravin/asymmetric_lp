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
        initial_token0 = 1000.0
        initial_token1 = 0.5
        current_token0 = 1000.0
        current_token1 = 0.5
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            initial_token0=initial_token0,
            initial_token1=initial_token1,
            current_token0=current_token0,
            current_token1=current_token1
        )
        
        assert result is False
    
    def test_should_rebalance_token0_depleted(self):
        """Test that rebalance occurs when token0 is depleted beyond threshold."""
        current_price = 3000.0
        price_history = []
        initial_token0 = 1000.0
        initial_token1 = 0.5
        current_token0 = 500.0  # 50% depleted (above 40% threshold)
        current_token1 = 0.5
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            initial_token0=initial_token0,
            initial_token1=initial_token1,
            current_token0=current_token0,
            current_token1=current_token1
        )
        
        assert result is True
    
    def test_should_rebalance_token1_depleted(self):
        """Test that rebalance occurs when token1 is depleted beyond threshold."""
        current_price = 3000.0
        price_history = []
        initial_token0 = 1000.0
        initial_token1 = 0.5
        current_token0 = 1000.0
        current_token1 = 0.2  # 60% depleted (above 40% threshold)
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            initial_token0=initial_token0,
            initial_token1=initial_token1,
            current_token0=current_token0,
            current_token1=current_token1
        )
        
        assert result is True
    
    def test_should_rebalance_both_tokens_depleted(self):
        """Test that rebalance occurs when both tokens are depleted."""
        current_price = 3000.0
        price_history = []
        initial_token0 = 1000.0
        initial_token1 = 0.5
        current_token0 = 600.0  # 40% depleted (at threshold)
        current_token1 = 0.3  # 40% depleted (at threshold)
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            initial_token0=initial_token0,
            initial_token1=initial_token1,
            current_token0=current_token0,
            current_token1=current_token1
        )
        
        assert result is True
    
    def test_should_rebalance_edge_case_zero_balances(self):
        """Test edge case with zero initial balances."""
        current_price = 3000.0
        price_history = []
        initial_token0 = 0.0
        initial_token1 = 0.0
        current_token0 = 0.0
        current_token1 = 0.0
        
        result = self.strategy.should_rebalance(
            current_price=current_price,
            price_history=price_history,
            initial_token0=initial_token0,
            initial_token1=initial_token1,
            current_token0=current_token0,
            current_token1=current_token1
        )
        
        assert result is False  # Should not rebalance with zero balances
    
    def test_plan_rebalance_basic(self):
        """Test basic rebalance planning."""
        current_price = 3000.0
        price_history = []
        token0_balance = 1000.0
        token1_balance = 0.5
        initial_target_ratio = 0.5
        
        # Mock the inventory model
        self.inventory_model.calculate_range_width.return_value = 0.1
        
        result = self.strategy.plan_rebalance(
            current_price=current_price,
            price_history=price_history,
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            initial_target_ratio=initial_target_ratio
        )
        
        assert result is not None
        assert 'lower_tick' in result
        assert 'upper_tick' in result
        assert 'fee_tier' in result
    
    def test_plan_rebalance_with_price_history(self):
        """Test rebalance planning with price history."""
        current_price = 3000.0
        price_history = [
            {'price': 2900.0, 'timestamp': 1000},
            {'price': 2950.0, 'timestamp': 2000},
            {'price': 3000.0, 'timestamp': 3000}
        ]
        token0_balance = 1000.0
        token1_balance = 0.5
        initial_target_ratio = 0.5
        
        # Mock the inventory model
        self.inventory_model.calculate_range_width.return_value = 0.1
        
        result = self.strategy.plan_rebalance(
            current_price=current_price,
            price_history=price_history,
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            initial_target_ratio=initial_target_ratio
        )
        
        assert result is not None
        assert 'lower_tick' in result
        assert 'upper_tick' in result


class TestAvellanedaStoikovModel:
    """Test the Avellaneda-Stoikov model."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.VOLATILITY = 0.3
        self.config.RISK_AVERSION = 0.1
        self.config.INVENTORY_PENALTY = 0.01
        
        self.model = AvellanedaStoikovModel(self.config)
    
    def test_calculate_range_width_basic(self):
        """Test basic range width calculation."""
        inventory_ratio = 0.5
        volatility = 0.3
        
        result = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result, float)
        assert result > 0
        assert result <= 1.0  # Should be a percentage
    
    def test_calculate_range_width_edge_cases(self):
        """Test edge cases for range width calculation."""
        volatility = 0.3
        
        # Test with extreme inventory ratios
        result_min = self.model.calculate_range_width(0.0, volatility)
        result_max = self.model.calculate_range_width(1.0, volatility)
        
        assert isinstance(result_min, float)
        assert isinstance(result_max, float)
        assert result_min > 0
        assert result_max > 0


class TestGLFTModel:
    """Test the GLFT model."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        
        self.model = GLFTModel(self.config)
    
    def test_calculate_range_width_basic(self):
        """Test basic range width calculation."""
        inventory_ratio = 0.5
        volatility = 0.3
        
        result = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result, float)
        assert result > 0
        assert result <= 1.0  # Should be a percentage
    
    def test_calculate_range_width_with_execution_cost(self):
        """Test range width calculation with execution cost."""
        inventory_ratio = 0.5
        volatility = 0.3
        
        result = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result, float)
        assert result > 0


if __name__ == "__main__":
    pytest.main([__file__])
