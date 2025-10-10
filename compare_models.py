#!/usr/bin/env python3
"""
Compare AS and GLFT models by showing their range calculations.
"""
import pandas as pd
import numpy as np
from config import Config
from models.model_factory import ModelFactory
import json

def compare_models():
    """Compare AS and GLFT models with sample data."""
    
    # Load sample OHLC data
    df = pd.read_csv('btc_sample_7days.csv')
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    
    # Create price history format expected by models
    price_history = []
    for _, row in df.head(100).iterrows():  # Use first 100 points
        price_history.append({
            'timestamp': row['timestamp'],
            'price': row['close'],
            'open': row['open'],
            'high': row['high'],
            'low': row['low'],
            'volume': row['volume']
        })
    
    # Sample parameters
    token0_balance = 10000 * 10**18  # 10000 tokens in wei
    token1_balance = 0.2 * 10**18    # 0.2 tokens in wei
    spot_price = df['close'].iloc[-1]  # Latest price
    
    # Mock client for token decimals
    class MockClient:
        def get_token_decimals(self, address):
            return 18
    
    mock_client = MockClient()
    config = Config()
    
    print("🔍 Model Comparison: AS vs GLFT")
    print("=" * 50)
    print(f"📊 Sample Data:")
    print(f"   Token0 Balance: {token0_balance / 10**18:.2f}")
    print(f"   Token1 Balance: {token1_balance / 10**18:.2f}")
    print(f"   Spot Price: ${spot_price:.2f}")
    print(f"   Price History Points: {len(price_history)}")
    print()
    
    # Test AS Model
    print("📈 Avellaneda-Stoikov Model:")
    print("-" * 30)
    as_model = ModelFactory.create_model('AvellanedaStoikovModel', config)
    as_result = as_model.calculate_lp_ranges(
        token0_balance, token1_balance, spot_price, price_history,
        "0xTokenA", "0xTokenB", mock_client
    )
    
    print(f"   Range A: {as_result['range_a_percentage']:.2f}%")
    print(f"   Range B: {as_result['range_b_percentage']:.2f}%")
    print(f"   Inventory Ratio: {as_result['inventory_ratio']:.3f}")
    print(f"   Deviation: {as_result['deviation']:.3f}")
    print(f"   Volatility: {as_result['volatility']:.4f}")
    print(f"   Model: {as_result['model_metadata']['model_name']}")
    print()
    
    # Test GLFT Model
    print("📈 GLFT Model:")
    print("-" * 30)
    glft_model = ModelFactory.create_model('GLFTModel', config)
    glft_result = glft_model.calculate_lp_ranges(
        token0_balance, token1_balance, spot_price, price_history,
        "0xTokenA", "0xTokenB", mock_client
    )
    
    print(f"   Range A: {glft_result['range_a_percentage']:.2f}%")
    print(f"   Range B: {glft_result['range_b_percentage']:.2f}%")
    print(f"   Inventory Ratio: {glft_result['inventory_ratio']:.3f}")
    print(f"   Deviation: {glft_result['deviation']:.3f}")
    print(f"   Volatility: {glft_result['volatility']:.4f}")
    print(f"   Model: {glft_result['model_metadata']['model_name']}")
    print(f"   Execution Cost: {glft_result['model_metadata']['execution_cost']:.4f}")
    print(f"   Inventory Penalty: {glft_result['model_metadata']['inventory_penalty']:.3f}")
    print()
    
    # Compare results
    print("🔍 Comparison:")
    print("-" * 30)
    range_a_diff = glft_result['range_a_percentage'] - as_result['range_a_percentage']
    range_b_diff = glft_result['range_b_percentage'] - as_result['range_b_percentage']
    
    print(f"   Range A Difference: {range_a_diff:+.2f}% (GLFT - AS)")
    print(f"   Range B Difference: {range_b_diff:+.2f}% (GLFT - AS)")
    
    if abs(range_a_diff) > 0.1 or abs(range_b_diff) > 0.1:
        print("   ✅ Models show different behavior!")
    else:
        print("   ⚠️  Models show similar behavior in this scenario")
    
    print()
    
    # Test with different inventory scenarios
    print("🧪 Testing Different Inventory Scenarios:")
    print("-" * 40)
    
    scenarios = [
        ("Balanced", 5000 * 10**18, 0.1 * 10**18),
        ("Token0 Heavy", 15000 * 10**18, 0.05 * 10**18),
        ("Token1 Heavy", 2000 * 10**18, 0.3 * 10**18),
    ]
    
    for scenario_name, bal0, bal1 in scenarios:
        print(f"\n📊 {scenario_name} Scenario:")
        print(f"   Token0: {bal0 / 10**18:.0f}, Token1: {bal1 / 10**18:.2f}")
        
        # AS Model
        as_result = as_model.calculate_lp_ranges(
            bal0, bal1, spot_price, price_history,
            "0xTokenA", "0xTokenB", mock_client
        )
        
        # GLFT Model
        glft_result = glft_model.calculate_lp_ranges(
            bal0, bal1, spot_price, price_history,
            "0xTokenA", "0xTokenB", mock_client
        )
        
        print(f"   AS  - Range A: {as_result['range_a_percentage']:6.2f}%, Range B: {as_result['range_b_percentage']:6.2f}%")
        print(f"   GLFT - Range A: {glft_result['range_a_percentage']:6.2f}%, Range B: {glft_result['range_b_percentage']:6.2f}%")
        
        diff_a = glft_result['range_a_percentage'] - as_result['range_a_percentage']
        diff_b = glft_result['range_b_percentage'] - as_result['range_b_percentage']
        print(f"   Diff - Range A: {diff_a:+6.2f}%, Range B: {diff_b:+6.2f}%")
    
    print("\n" + "=" * 50)
    print("✅ Model comparison completed!")

if __name__ == "__main__":
    compare_models()
