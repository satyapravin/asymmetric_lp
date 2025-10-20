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
    
    @patch('backtest_engine.AsymmetricLPStrategy')
    @patch('backtest_engine.AvellanedaStoikovModel')
    def test_backtest_engine_initialization(self, mock_model_class, mock_strategy_class):
        """Test backtest engine initialization."""
        mock_model = Mock()
        mock_strategy = Mock()
        mock_model_class.return_value = mock_model
        mock_strategy_class.return_value = mock_strategy
        
        engine = BacktestEngine(
            config=self.config,
            ohlc_data=self.sample_data,
            model_type='avellaneda_stoikov'
        )
        
        assert engine.config == self.config
        assert len(engine.ohlc_data) > 0
        assert engine.model == mock_model
        assert engine.strategy == mock_strategy
    
    @patch('backtest_engine.AsymmetricLPStrategy')
    @patch('backtest_engine.AvellanedaStoikovModel')
    def test_backtest_engine_run_basic(self, mock_model_class, mock_strategy_class):
        """Test basic backtest engine run."""
        mock_model = Mock()
        mock_strategy = Mock()
        mock_model_class.return_value = mock_model
        mock_strategy_class.return_value = mock_strategy
        
        # Configure strategy mock
        mock_strategy.should_rebalance.return_value = False
        mock_strategy.plan_rebalance.return_value = {
            'lower_tick': 1000,
            'upper_tick': 2000,
            'fee_tier': 5
        }
        
        engine = BacktestEngine(
            config=self.config,
            ohlc_data=self.sample_data,
            model_type='avellaneda_stoikov'
        )
        
        # Mock the AMM
        mock_amm = Mock()
        mock_amm.mint_position.return_value = {'success': True}
        mock_amm.collect_fees.return_value = {'token0': 0.1, 'token1': 0.0001}
        
        with patch('backtest_engine.AMM', return_value=mock_amm):
            result = engine.run()
        
        assert result is not None
        assert 'final_balance_0' in result
        assert 'final_balance_1' in result
        assert 'total_trades' in result
        assert 'total_rebalances' in result
    
    @patch('backtest_engine.AsymmetricLPStrategy')
    @patch('backtest_engine.AvellanedaStoikovModel')
    def test_backtest_engine_rebalance_trigger(self, mock_model_class, mock_strategy_class):
        """Test backtest engine with rebalance trigger."""
        mock_model = Mock()
        mock_strategy = Mock()
        mock_model_class.return_value = mock_model
        mock_strategy_class.return_value = mock_strategy
        
        # Configure strategy to trigger rebalance after some time
        call_count = 0
        def should_rebalance_side_effect(*args, **kwargs):
            nonlocal call_count
            call_count += 1
            return call_count > 10  # Trigger rebalance after 10 calls
        
        mock_strategy.should_rebalance.side_effect = should_rebalance_side_effect
        mock_strategy.plan_rebalance.return_value = {
            'lower_tick': 1000,
            'upper_tick': 2000,
            'fee_tier': 5
        }
        
        engine = BacktestEngine(
            config=self.config,
            ohlc_data=self.sample_data,
            model_type='avellaneda_stoikov'
        )
        
        # Mock the AMM
        mock_amm = Mock()
        mock_amm.mint_position.return_value = {'success': True}
        mock_amm.collect_fees.return_value = {'token0': 0.1, 'token1': 0.0001}
        
        with patch('backtest_engine.AMM', return_value=mock_amm):
            result = engine.run()
        
        assert result is not None
        assert result['total_rebalances'] > 0  # Should have triggered rebalances
    
    def test_backtest_engine_data_validation(self):
        """Test backtest engine data validation."""
        # Test with invalid data
        invalid_data = pd.DataFrame({
            'timestamp': [1000, 2000],
            'open': [3000, 3100],
            'high': [3050, 3150],
            'low': [2950, 3050],
            'close': [3000, 3100],
            'volume': [100, 200]
        })
        
        with pytest.raises(ValueError):
            BacktestEngine(
                config=self.config,
                ohlc_data=invalid_data,
                model_type='avellaneda_stoikov'
            )
    
    def test_backtest_engine_performance_metrics(self):
        """Test that backtest engine calculates performance metrics correctly."""
        mock_model = Mock()
        mock_strategy = Mock()
        
        with patch('backtest_engine.AvellanedaStoikovModel', return_value=mock_model), \
             patch('backtest_engine.AsymmetricLPStrategy', return_value=mock_strategy):
            
            mock_strategy.should_rebalance.return_value = False
            mock_strategy.plan_rebalance.return_value = {
                'lower_tick': 1000,
                'upper_tick': 2000,
                'fee_tier': 5
            }
            
            engine = BacktestEngine(
                config=self.config,
                ohlc_data=self.sample_data,
                model_type='avellaneda_stoikov'
            )
            
            # Mock the AMM
            mock_amm = Mock()
            mock_amm.mint_position.return_value = {'success': True}
            mock_amm.collect_fees.return_value = {'token0': 0.1, 'token1': 0.0001}
            
            with patch('backtest_engine.AMM', return_value=mock_amm):
                result = engine.run()
            
            # Check that performance metrics are calculated
            assert 'total_return_percent' in result
            assert 'total_fees_collected' in result
            assert 'max_drawdown' in result
            assert isinstance(result['total_return_percent'], float)
            assert isinstance(result['total_fees_collected'], float)


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
        current_price = 3000.0
        price_history = []
        token0_balance = 1000.0
        token1_balance = 0.5
        initial_target_ratio = 0.5
        
        # Test that strategy calls the model
        result = self.strategy.plan_rebalance(
            current_price=current_price,
            price_history=price_history,
            token0_balance=token0_balance,
            token1_balance=token1_balance,
            initial_target_ratio=initial_target_ratio
        )
        
        # Verify that the model was called
        self.inventory_model.calculate_range_width.assert_called()
        
        assert result is not None
        assert 'lower_tick' in result
        assert 'upper_tick' in result
    
    def test_strategy_with_different_models(self):
        """Test strategy with different inventory models."""
        from models.glft_model import GLFTModel
        from models.simple_model import SimpleModel
        
        # Test with GLFT model
        glft_model = Mock()
        glft_model.calculate_range_width.return_value = 0.2
        
        glft_strategy = AsymmetricLPStrategy(
            config=self.config,
            inventory_model=glft_model
        )
        
        result_glft = glft_strategy.plan_rebalance(
            current_price=3000.0,
            price_history=[],
            token0_balance=1000.0,
            token1_balance=0.5,
            initial_target_ratio=0.5
        )
        
        assert result_glft is not None
        glft_model.calculate_range_width.assert_called()
        
        # Test with Simple model
        simple_model = Mock()
        simple_model.calculate_range_width.return_value = 0.15
        
        simple_strategy = AsymmetricLPStrategy(
            config=self.config,
            inventory_model=simple_model
        )
        
        result_simple = simple_strategy.plan_rebalance(
            current_price=3000.0,
            price_history=[],
            token0_balance=1000.0,
            token1_balance=0.5,
            initial_target_ratio=0.5
        )
        
        assert result_simple is not None
        simple_model.calculate_range_width.assert_called()


if __name__ == "__main__":
    pytest.main([__file__])
