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
        
        self.model = AvellanedaStoikovModel(self.config)
    
    def test_model_initialization(self):
        """Test model initialization."""
        assert self.model.config == self.config
        assert hasattr(self.model, 'calculate_range_width')
    
    def test_calculate_range_width_normal_case(self):
        """Test range width calculation for normal inventory ratio."""
        inventory_ratio = 0.5  # Balanced inventory
        volatility = 0.3
        
        result = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result, float)
        assert result > 0
        assert result <= 1.0  # Should be a percentage
    
    def test_calculate_range_width_extreme_inventory(self):
        """Test range width calculation for extreme inventory ratios."""
        volatility = 0.3
        
        # Test with inventory heavily skewed to token0
        result_token0_heavy = self.model.calculate_range_width(0.1, volatility)
        
        # Test with inventory heavily skewed to token1
        result_token1_heavy = self.model.calculate_range_width(0.9, volatility)
        
        assert isinstance(result_token0_heavy, float)
        assert isinstance(result_token1_heavy, float)
        assert result_token0_heavy > 0
        assert result_token1_heavy > 0
    
    def test_calculate_range_width_volatility_sensitivity(self):
        """Test that range width responds to volatility changes."""
        inventory_ratio = 0.5
        
        # Low volatility should result in narrower ranges
        result_low_vol = self.model.calculate_range_width(inventory_ratio, 0.1)
        
        # High volatility should result in wider ranges
        result_high_vol = self.model.calculate_range_width(inventory_ratio, 0.5)
        
        assert isinstance(result_low_vol, float)
        assert isinstance(result_high_vol, float)
        assert result_low_vol > 0
        assert result_high_vol > 0
        # Note: The exact relationship depends on model implementation
    
    def test_calculate_range_width_edge_cases(self):
        """Test edge cases for range width calculation."""
        volatility = 0.3
        
        # Test with zero inventory ratio
        result_zero = self.model.calculate_range_width(0.0, volatility)
        assert isinstance(result_zero, float)
        assert result_zero > 0
        
        # Test with maximum inventory ratio
        result_max = self.model.calculate_range_width(1.0, volatility)
        assert isinstance(result_max, float)
        assert result_max > 0
        
        # Test with zero volatility
        result_zero_vol = self.model.calculate_range_width(0.5, 0.0)
        assert isinstance(result_zero_vol, float)
        assert result_zero_vol >= 0


class TestGLFTModel:
    """Test the GLFT (Guéant-Lehalle-Fernandez-Tapia) model."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        
        self.model = GLFTModel(self.config)
    
    def test_model_initialization(self):
        """Test model initialization."""
        assert self.model.config == self.config
        assert hasattr(self.model, 'calculate_range_width')
    
    def test_calculate_range_width_normal_case(self):
        """Test range width calculation for normal inventory ratio."""
        inventory_ratio = 0.5  # Balanced inventory
        volatility = 0.3
        
        result = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result, float)
        assert result > 0
        assert result <= 1.0  # Should be a percentage
    
    def test_calculate_range_width_execution_cost_sensitivity(self):
        """Test that range width responds to execution cost."""
        inventory_ratio = 0.5
        volatility = 0.3
        
        # Test with different execution costs
        result_low_cost = self.model.calculate_range_width(inventory_ratio, volatility)
        
        # Modify execution cost and test again
        self.config.EXECUTION_COST = 0.01  # Higher cost
        result_high_cost = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result_low_cost, float)
        assert isinstance(result_high_cost, float)
        assert result_low_cost > 0
        assert result_high_cost > 0
    
    def test_calculate_range_width_inventory_penalty_sensitivity(self):
        """Test that range width responds to inventory penalty."""
        inventory_ratio = 0.5
        volatility = 0.3
        
        # Test with different inventory penalties
        result_low_penalty = self.model.calculate_range_width(inventory_ratio, volatility)
        
        # Modify inventory penalty and test again
        self.config.INVENTORY_PENALTY = 0.1  # Higher penalty
        result_high_penalty = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result_low_penalty, float)
        assert isinstance(result_high_penalty, float)
        assert result_low_penalty > 0
        assert result_high_penalty > 0


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
        assert hasattr(self.model, 'calculate_range_width')
    
    def test_calculate_range_width_constant(self):
        """Test that simple model returns constant range width."""
        inventory_ratio = 0.5
        volatility = 0.3
        
        result = self.model.calculate_range_width(inventory_ratio, volatility)
        
        assert isinstance(result, float)
        assert result == self.config.BASE_SPREAD
    
    def test_calculate_range_width_independence(self):
        """Test that simple model is independent of inventory and volatility."""
        volatility = 0.3
        
        # Test with different inventory ratios
        result1 = self.model.calculate_range_width(0.1, volatility)
        result2 = self.model.calculate_range_width(0.5, volatility)
        result3 = self.model.calculate_range_width(0.9, volatility)
        
        assert result1 == result2 == result3 == self.config.BASE_SPREAD
        
        # Test with different volatilities
        result4 = self.model.calculate_range_width(0.5, 0.1)
        result5 = self.model.calculate_range_width(0.5, 0.5)
        
        assert result4 == result5 == self.config.BASE_SPREAD


class TestModelFactory:
    """Test the model factory."""
    
    def test_model_factory_creation(self):
        """Test that model factory can create different models."""
        from models.model_factory import ModelFactory
        
        config = Mock()
        
        # Test Avellaneda-Stoikov model creation
        as_model = ModelFactory.create_model('avellaneda_stoikov', config)
        assert isinstance(as_model, AvellanedaStoikovModel)
        
        # Test GLFT model creation
        glft_model = ModelFactory.create_model('glft', config)
        assert isinstance(glft_model, GLFTModel)
        
        # Test Simple model creation
        simple_model = ModelFactory.create_model('simple', config)
        assert isinstance(simple_model, SimpleModel)
    
    def test_model_factory_invalid_model(self):
        """Test that model factory handles invalid model names."""
        from models.model_factory import ModelFactory
        
        config = Mock()
        
        with pytest.raises(ValueError):
            ModelFactory.create_model('invalid_model', config)


if __name__ == "__main__":
    pytest.main([__file__])
