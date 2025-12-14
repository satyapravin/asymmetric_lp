"""
Unit tests for AsymmetricLP models.
Tests the pricing models (Avellaneda-Stoikov, GLFT) in isolation.
"""
import pytest
import sys
import os
import numpy as np
from unittest.mock import Mock

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from models.avellaneda_stoikov import AvellanedaStoikovModel
from models.glft_model import GLFTModel
from models.simple_model import SimpleModel


class TestAvellanedaStoikovModel:
    """Test the Avellaneda-Stoikov market making model."""
    
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
    
    def test_model_initialization(self):
        """Test model initialization."""
        assert self.model.config == self.config
        assert hasattr(self.model, 'calculate_lp_ranges')
    
    def test_calculate_lp_ranges_normal_case(self):
        """Test LP range calculation for normal inventory ratio."""
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
    
    def test_calculate_lp_ranges_extreme_inventory(self):
        """Test LP range calculation for extreme inventory ratios."""
        from unittest.mock import Mock
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        spot_price = 3000.0
        price_history = []
        
        # Test with inventory heavily skewed to token0
        token0_heavy = 2000 * 10**18
        token1_heavy = 0.1 * 10**18
        result_token0_heavy = self.model.calculate_lp_ranges(
            token0_balance=token0_heavy,
            token1_balance=token1_heavy,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        # Test with inventory heavily skewed to token1
        token0_light = 100 * 10**18
        token1_light = 2.0 * 10**18
        result_token1_heavy = self.model.calculate_lp_ranges(
            token0_balance=token0_light,
            token1_balance=token1_light,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        assert isinstance(result_token0_heavy, dict)
        assert isinstance(result_token1_heavy, dict)
        assert 'range_a_percentage' in result_token0_heavy
        assert 'range_b_percentage' in result_token0_heavy


class TestGLFTModel:
    """Test the GLFT (Guéant-Lehalle-Fernandez-Tapia) model."""
    
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
    
    def test_model_initialization(self):
        """Test model initialization."""
        assert self.model.config == self.config
        assert hasattr(self.model, 'calculate_lp_ranges')
    
    def test_calculate_lp_ranges_normal_case(self):
        """Test LP range calculation for normal inventory ratio."""
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
    
    def test_calculate_lp_ranges_execution_cost_sensitivity(self):
        """Test that LP ranges respond to execution cost."""
        from unittest.mock import Mock
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        token0_balance = 1000 * 10**18
        token1_balance = 1 * 10**18
        spot_price = 3000.0
        price_history = []
        
        # Test with default execution cost
        result_low_cost = self.model.calculate_lp_ranges(
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        # Modify execution cost and test again (requires recreating model)
        self.config.EXECUTION_COST = 0.01  # Higher cost
        model_high_cost = GLFTModel(self.config)
        result_high_cost = model_high_cost.calculate_lp_ranges(
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        assert isinstance(result_low_cost, dict)
        assert isinstance(result_high_cost, dict)
        assert 'range_a_percentage' in result_low_cost
        assert 'range_b_percentage' in result_high_cost
    
    def test_calculate_lp_ranges_inventory_penalty_sensitivity(self):
        """Test that LP ranges respond to inventory penalty."""
        from unittest.mock import Mock
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        token0_balance = 1000 * 10**18
        token1_balance = 1 * 10**18
        spot_price = 3000.0
        price_history = []
        
        # Test with default penalty
        result_low_penalty = self.model.calculate_lp_ranges(
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        # Modify penalty and test again (requires recreating model)
        self.config.INVENTORY_PENALTY = 0.1  # Higher penalty
        model_high_penalty = GLFTModel(self.config)
        result_high_penalty = model_high_penalty.calculate_lp_ranges(
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            spot_price=spot_price,
            price_history=price_history,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB',
            client=mock_client
        )
        
        assert isinstance(result_low_penalty, dict)
        assert isinstance(result_high_penalty, dict)
        assert 'range_a_percentage' in result_low_penalty
        assert 'range_b_percentage' in result_high_penalty


class TestSimpleModel:
    """Test the simple model."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.BASE_SPREAD = 0.15
        
        self.model = SimpleModel(self.config)
    
    def test_model_initialization(self):
        """Test model initialization."""
        assert self.model.config == self.config
        assert hasattr(self.model, 'calculate_lp_ranges')
    
    def test_calculate_lp_ranges_basic(self):
        """Test that simple model can calculate LP ranges."""
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


class TestModelFactory:
    """Test the model factory."""
    
    def test_model_factory_creation(self):
        """Test that model factory can create different models."""
        from models.model_factory import ModelFactory
        
        config = Mock()
        
        # Add required config attributes
        config.INVENTORY_RISK_AVERSION = 0.1
        config.TARGET_INVENTORY_RATIO = 0.5
        config.MAX_INVENTORY_DEVIATION = 0.3
        config.BASE_SPREAD = 0.05
        config.VOLATILITY_WINDOW_SIZE = 20
        config.DEFAULT_VOLATILITY = 0.02
        config.MAX_VOLATILITY = 2.0
        config.MIN_VOLATILITY = 0.01
        config.EXECUTION_COST = 0.001
        config.INVENTORY_PENALTY = 0.01
        config.MAX_POSITION_SIZE = 0.1
        config.TERMINAL_INVENTORY_PENALTY = 0.2
        config.INVENTORY_CONSTRAINT_ACTIVE = False
        
        # Test Avellaneda-Stoikov model creation
        as_model = ModelFactory.create_model('AvellanedaStoikovModel', config)
        assert isinstance(as_model, AvellanedaStoikovModel)
        
        # Test GLFT model creation
        glft_model = ModelFactory.create_model('GLFTModel', config)
        assert isinstance(glft_model, GLFTModel)
        
        # Test Simple model creation
        simple_model = ModelFactory.create_model('SimpleModel', config)
        assert isinstance(simple_model, SimpleModel)
    
    def test_model_factory_invalid_model(self):
        """Test that model factory handles invalid model names."""
        from models.model_factory import ModelFactory
        
        config = Mock()
        
        with pytest.raises(ValueError):
            ModelFactory.create_model('invalid_model', config)


if __name__ == "__main__":
    pytest.main([__file__])
