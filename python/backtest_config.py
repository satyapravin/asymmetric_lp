"""
Backtesting-specific configuration.
Loads from backtest_config.json (separate from production .env)
"""
import json
import os
from typing import Dict, Any
from dataclasses import dataclass


@dataclass
class BacktestConfig:
    """Backtesting-specific configuration loaded from JSON file"""
    risk_free_rate: float = 0.02
    default_daily_volatility: float = 0.02
    default_initial_balance_0: float = 3000.0
    default_initial_balance_1: float = 1.0
    trade_detection_threshold: float = 0.0005
    position_falloff_factor: float = 0.1
    
    @classmethod
    def load(cls, config_file: str = 'backtest_config.json') -> 'BacktestConfig':
        """
        Load backtest configuration from JSON file
        
        Args:
            config_file: Path to JSON config file
            
        Returns:
            BacktestConfig instance
        """
        # Try to load from specified file
        if os.path.exists(config_file):
            with open(config_file, 'r') as f:
                data = json.load(f)
                return cls(**data)
        
        # Try example file as fallback
        example_file = 'backtest_config.example.json'
        if os.path.exists(example_file):
            with open(example_file, 'r') as f:
                data = json.load(f)
                return cls(**data)
        
        # Return defaults if no file found
        return cls()
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return {
            'risk_free_rate': self.risk_free_rate,
            'default_daily_volatility': self.default_daily_volatility,
            'default_initial_balance_0': self.default_initial_balance_0,
            'default_initial_balance_1': self.default_initial_balance_1,
            'trade_detection_threshold': self.trade_detection_threshold,
            'position_falloff_factor': self.position_falloff_factor
        }
    
    def save(self, config_file: str = 'backtest_config.json'):
        """Save configuration to JSON file"""
        with open(config_file, 'w') as f:
            json.dump(self.to_dict(), f, indent=2)

