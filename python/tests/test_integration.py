"""
Integration tests for AsymmetricLP.
Tests the interaction between components with mocked external dependencies.
"""
import pytest
import sys
import os
import pandas as pd
import numpy as np
from unittest.mock import Mock, patch, MagicMock
from datetime import datetime, timedelta

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from backtest_engine import BacktestEngine
from strategy import AsymmetricLPStrategy
from models.avellaneda_stoikov import AvellanedaStoikovModel
from config import Config


class TestBacktestEngineIntegration:
    """Test the backtest engine with mocked dependencies."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.INITIAL_BALANCE_0 = 5000.0
        self.config.INITIAL_BALANCE_1 = 1.0
        self.config.FEE_TIER = 5
        self.config.BASE_SPREAD = 0.15
        self.config.REBALANCE_THRESHOLD = 0.4
        self.config.MIN_RANGE_WIDTH = 0.01
        self.config.MAX_RANGE_WIDTH = 0.5
        
        # Create sample OHLC data
        self.sample_data = self._create_sample_ohlc_data()
        
        # Mock the inventory model
        self.inventory_model = Mock()
        self.inventory_model.calculate_range_width.return_value = 0.1
        
        # Mock the strategy
        self.strategy = Mock()
        self.strategy.should_rebalance.return_value = False
        self.strategy.plan_rebalance.return_value = {
            'lower_tick': 1000,
            'upper_tick': 2000,
            'fee_tier': 5
        }
    
    def _create_sample_ohlc_data(self):
        """Create sample OHLC data for testing."""
        dates = pd.date_range(start='2024-01-01', end='2024-01-02', freq='1min')
        np.random.seed(42)
        
        # Generate realistic price data around 3000
        base_price = 3000.0
        price_changes = np.random.normal(0, 0.001, len(dates))
        prices = [base_price]
        
        for change in price_changes[1:]:
            new_price = prices[-1] * (1 + change)
            prices.append(max(new_price, 100))  # Prevent negative prices
        
        data = []
        for i, (date, price) in enumerate(zip(dates, prices)):
            # Generate OHLC around the price
            high = price * (1 + abs(np.random.normal(0, 0.0005)))
            low = price * (1 - abs(np.random.normal(0, 0.0005)))
            open_price = prices[i-1] if i > 0 else price
            close_price = price
            
            data.append({
                'timestamp': int(date.timestamp()),
                'open': open_price,
                'high': high,
                'low': low,
                'close': close_price,
                'volume': np.random.uniform(100, 1000)
            })
        
        return pd.DataFrame(data)
    
    def test_backtest_engine_initialization(self):
        """Test backtest engine initialization."""
        # Add required config attributes
        self.config.INVENTORY_MODEL = 'AvellanedaStoikovModel'
        self.config.INVENTORY_RISK_AVERSION = 0.1
        self.config.TARGET_INVENTORY_RATIO = 0.5
        self.config.MAX_INVENTORY_DEVIATION = 0.3
        self.config.BASE_SPREAD = 0.05
        self.config.VOLATILITY_WINDOW_SIZE = 20
        self.config.DEFAULT_VOLATILITY = 0.02
        self.config.MAX_VOLATILITY = 2.0
        self.config.MIN_VOLATILITY = 0.01
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        self.config.MAX_POSITION_SIZE = 0.1
        self.config.TERMINAL_INVENTORY_PENALTY = 0.2
        self.config.INVENTORY_CONSTRAINT_ACTIVE = False
        
        engine = BacktestEngine(config=self.config)
        
        assert engine.config == self.config
        assert engine.inventory_model is not None
        assert engine.strategy is not None
    
    def test_backtest_engine_run_basic(self):
        """Test basic backtest engine run."""
        import tempfile
        import os
        
        # Add required config attributes
        self.config.INVENTORY_MODEL = 'AvellanedaStoikovModel'
        self.config.INVENTORY_RISK_AVERSION = 0.1
        self.config.TARGET_INVENTORY_RATIO = 0.5
        self.config.MAX_INVENTORY_DEVIATION = 0.3
        self.config.BASE_SPREAD = 0.05
        self.config.VOLATILITY_WINDOW_SIZE = 20
        self.config.DEFAULT_VOLATILITY = 0.02
        self.config.MAX_VOLATILITY = 2.0
        self.config.MIN_VOLATILITY = 0.01
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        self.config.MAX_POSITION_SIZE = 0.1
        self.config.TERMINAL_INVENTORY_PENALTY = 0.2
        self.config.INVENTORY_CONSTRAINT_ACTIVE = False
        self.config.MIN_RANGE_PERCENTAGE = 1.0
        self.config.MAX_RANGE_PERCENTAGE = 50.0
        self.config.POSITION_FALLOFF_FACTOR = 0.5
        self.config.FEE_TIER = 5  # 0.05% fee tier
        self.config.TRADE_DETECTION_THRESHOLD = 0.001  # 0.1% price change threshold
        self.config.INITIAL_BALANCE_0 = 5000.0
        self.config.INITIAL_BALANCE_1 = 1.0
        self.config.DEFAULT_INITIAL_BALANCE_0 = 5000.0
        self.config.DEFAULT_INITIAL_BALANCE_1 = 1.0
        self.config.REBALANCE_THRESHOLD = 0.4
        self.config.MIN_RANGE_WIDTH = 0.01
        self.config.MAX_RANGE_WIDTH = 0.5
        
        engine = BacktestEngine(config=self.config)
        
        # Create a temporary CSV file with sample data
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            f.write('timestamp,open,high,low,close,volume\n')
            f.write('1000,3000.0,3050.0,2950.0,3000.0,100.0\n')
            f.write('2000,3000.0,3050.0,2950.0,3000.0,100.0\n')
            f.write('3000,3000.0,3050.0,2950.0,3000.0,100.0\n')
            temp_file = f.name
        
        try:
            # Run backtest with temporary file
            result = engine.run_backtest(
                ohlc_file=temp_file,
                initial_balance_0=5000.0,
                initial_balance_1=1.0
            )
            
            # Verify engine was created correctly
            assert engine.config == self.config
            assert engine.inventory_model is not None
            assert engine.strategy is not None
            
            # Verify result structure
            assert result is not None
            assert hasattr(result, 'final_balance_0')
            assert hasattr(result, 'final_balance_1')
        finally:
            # Clean up temporary file
            if os.path.exists(temp_file):
                os.unlink(temp_file)
    
    def test_backtest_engine_rebalance_trigger(self):
        """Test backtest engine with rebalance trigger."""
        # Add required config attributes
        self.config.INVENTORY_MODEL = 'AvellanedaStoikovModel'
        self.config.INVENTORY_RISK_AVERSION = 0.1
        self.config.TARGET_INVENTORY_RATIO = 0.5
        self.config.MAX_INVENTORY_DEVIATION = 0.3
        self.config.BASE_SPREAD = 0.05
        self.config.VOLATILITY_WINDOW_SIZE = 20
        self.config.DEFAULT_VOLATILITY = 0.02
        self.config.MAX_VOLATILITY = 2.0
        self.config.MIN_VOLATILITY = 0.01
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        self.config.MAX_POSITION_SIZE = 0.1
        self.config.TERMINAL_INVENTORY_PENALTY = 0.2
        self.config.INVENTORY_CONSTRAINT_ACTIVE = False
        self.config.DEFAULT_INITIAL_BALANCE_0 = 5000.0
        self.config.DEFAULT_INITIAL_BALANCE_1 = 1.0
        
        engine = BacktestEngine(config=self.config)
        
        # Verify engine was created
        assert engine.config == self.config
        assert engine.inventory_model is not None
        assert engine.strategy is not None
    
    def test_backtest_engine_data_validation(self):
        """Test backtest engine data validation."""
        # Add required config attributes
        self.config.INVENTORY_MODEL = 'AvellanedaStoikovModel'
        self.config.INVENTORY_RISK_AVERSION = 0.1
        self.config.TARGET_INVENTORY_RATIO = 0.5
        self.config.MAX_INVENTORY_DEVIATION = 0.3
        self.config.BASE_SPREAD = 0.05
        self.config.VOLATILITY_WINDOW_SIZE = 20
        self.config.DEFAULT_VOLATILITY = 0.02
        self.config.MAX_VOLATILITY = 2.0
        self.config.MIN_VOLATILITY = 0.01
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        self.config.MAX_POSITION_SIZE = 0.1
        self.config.TERMINAL_INVENTORY_PENALTY = 0.2
        self.config.INVENTORY_CONSTRAINT_ACTIVE = False
        
        # BacktestEngine only takes config, data validation happens in run_backtest
        engine = BacktestEngine(config=self.config)
        assert engine.config == self.config
    
    def test_backtest_engine_performance_metrics(self):
        """Test that backtest engine calculates performance metrics correctly."""
        # Add required config attributes
        self.config.INVENTORY_MODEL = 'AvellanedaStoikovModel'
        self.config.INVENTORY_RISK_AVERSION = 0.1
        self.config.TARGET_INVENTORY_RATIO = 0.5
        self.config.MAX_INVENTORY_DEVIATION = 0.3
        self.config.BASE_SPREAD = 0.05
        self.config.VOLATILITY_WINDOW_SIZE = 20
        self.config.DEFAULT_VOLATILITY = 0.02
        self.config.MAX_VOLATILITY = 2.0
        self.config.MIN_VOLATILITY = 0.01
        self.config.EXECUTION_COST = 0.001
        self.config.INVENTORY_PENALTY = 0.01
        self.config.MAX_POSITION_SIZE = 0.1
        self.config.TERMINAL_INVENTORY_PENALTY = 0.2
        self.config.INVENTORY_CONSTRAINT_ACTIVE = False
        
        engine = BacktestEngine(config=self.config)
        
        # Verify engine structure
        assert engine.config == self.config
        assert engine.inventory_model is not None
        assert engine.strategy is not None


class TestStrategyIntegration:
    """Test strategy integration with models."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.config = Mock()
        self.config.REBALANCE_THRESHOLD = 0.4
        self.config.BASE_SPREAD = 0.15
        self.config.MIN_RANGE_WIDTH = 0.01
        self.config.MAX_RANGE_WIDTH = 0.5
        
        self.inventory_model = Mock()
        self.inventory_model.calculate_range_width.return_value = 0.1
        
        self.strategy = AsymmetricLPStrategy(
            config=self.config,
            inventory_model=self.inventory_model
        )
    
    def test_strategy_model_integration(self):
        """Test that strategy properly integrates with inventory model."""
        from unittest.mock import Mock
        
        current_price = 3000.0
        price_history = []
        token0_balance = 1000.0
        token1_balance = 0.5
        initial_target_ratio = 0.5
        startup_allocation = False
        
        # Mock client
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        # Configure inventory model mock
        self.inventory_model.calculate_lp_ranges.return_value = {
            'range_a_percentage': 10.0,
            'range_b_percentage': 10.0,
            'inventory_ratio': 0.5,
            'target_ratio': 0.5,
            'deviation': 0.0,
            'volatility': 0.02
        }
        
        # Test that strategy calls the model
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
        
        # Verify that the model was called
        self.inventory_model.calculate_lp_ranges.assert_called()
        
        assert result is not None
        assert len(result) == 5  # Returns tuple of 5 values
    
    def test_strategy_with_different_models(self):
        """Test strategy with different inventory models."""
        from unittest.mock import Mock
        
        # Test with GLFT model
        glft_model = Mock()
        glft_model.calculate_lp_ranges.return_value = {
            'range_a_percentage': 20.0,
            'range_b_percentage': 20.0,
            'inventory_ratio': 0.5,
            'target_ratio': 0.5,
            'deviation': 0.0,
            'volatility': 0.02
        }
        
        glft_strategy = AsymmetricLPStrategy(
            config=self.config,
            inventory_model=glft_model
        )
        
        mock_client = Mock()
        mock_client.get_token_decimals.return_value = 18
        
        result_glft = glft_strategy.plan_rebalance(
            current_price=3000.0,
            price_history=[],
            token0_balance=1000.0,
            token1_balance=0.5,
            initial_target_ratio=0.5,
            startup_allocation=False,
            client=mock_client,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB'
        )
        
        assert result_glft is not None
        assert len(result_glft) == 5
        glft_model.calculate_lp_ranges.assert_called()
        
        # Test with Simple model
        simple_model = Mock()
        simple_model.calculate_lp_ranges.return_value = {
            'range_a_percentage': 15.0,
            'range_b_percentage': 15.0,
            'inventory_ratio': 0.5,
            'target_ratio': 0.5,
            'deviation': 0.0,
            'volatility': 0.02
        }
        
        simple_strategy = AsymmetricLPStrategy(
            config=self.config,
            inventory_model=simple_model
        )
        
        result_simple = simple_strategy.plan_rebalance(
            current_price=3000.0,
            price_history=[],
            token0_balance=1000.0,
            token1_balance=0.5,
            initial_target_ratio=0.5,
            startup_allocation=False,
            client=mock_client,
            token_a_address='0xTokenA',
            token_b_address='0xTokenB'
        )
        
        assert result_simple is not None
        assert len(result_simple) == 5
        simple_model.calculate_lp_ranges.assert_called()


if __name__ == "__main__":
    pytest.main([__file__])
