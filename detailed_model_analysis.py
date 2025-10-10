#!/usr/bin/env python3
"""
Detailed analysis of AS vs GLFT models over time.
"""
import pandas as pd
import numpy as np
from config import Config
from models.model_factory import ModelFactory
import matplotlib.pyplot as plt
import json

def analyze_models_over_time():
    """Analyze how models behave over time with different market conditions."""
    
    # Load sample OHLC data
    df = pd.read_csv('btc_sample_7days.csv')
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    
    # Mock client for token decimals
    class MockClient:
        def get_token_decimals(self, address):
            return 18
    
    mock_client = MockClient()
    config = Config()
    
    # Initialize models
    as_model = ModelFactory.create_model('AvellanedaStoikovModel', config)
    glft_model = ModelFactory.create_model('GLFTModel', config)
    
    # Sample every 100th data point for analysis
    sample_indices = range(0, len(df), 100)
    
    results = []
    
    print("🔍 Detailed Model Analysis Over Time")
    print("=" * 50)
    
    for i, idx in enumerate(sample_indices):
        if idx >= len(df):
            break
            
        # Get current price and build price history
        current_price = df.iloc[idx]['close']
        price_history = []
        
        # Build price history from previous 50 points
        start_idx = max(0, idx - 50)
        for j in range(start_idx, idx + 1):
            price_history.append({
                'timestamp': df.iloc[j]['timestamp'],
                'price': df.iloc[j]['close'],
                'open': df.iloc[j]['open'],
                'high': df.iloc[j]['high'],
                'low': df.iloc[j]['low'],
                'volume': df.iloc[j]['volume']
            })
        
        # Test different inventory scenarios
        scenarios = [
            ("Balanced", 5000 * 10**18, 0.1 * 10**18),
            ("Token0 Heavy", 15000 * 10**18, 0.05 * 10**18),
            ("Token1 Heavy", 2000 * 10**18, 0.3 * 10**18),
        ]
        
        for scenario_name, bal0, bal1 in scenarios:
            # Calculate AS ranges
            as_result = as_model.calculate_lp_ranges(
                bal0, bal1, current_price, price_history,
                "0xTokenA", "0xTokenB", mock_client
            )
            
            # Calculate GLFT ranges
            glft_result = glft_model.calculate_lp_ranges(
                bal0, bal1, current_price, price_history,
                "0xTokenA", "0xTokenB", mock_client
            )
            
            results.append({
                'timestamp': df.iloc[idx]['timestamp'],
                'price': current_price,
                'scenario': scenario_name,
                'as_range_a': as_result['range_a_percentage'],
                'as_range_b': as_result['range_b_percentage'],
                'glft_range_a': glft_result['range_a_percentage'],
                'glft_range_b': glft_result['range_b_percentage'],
                'volatility': as_result['volatility'],
                'inventory_ratio': as_result['inventory_ratio']
            })
    
    # Convert to DataFrame for analysis
    results_df = pd.DataFrame(results)
    
    print(f"📊 Analyzed {len(results_df)} data points")
    print()
    
    # Summary statistics
    print("📈 Summary Statistics:")
    print("-" * 30)
    
    for scenario in ["Balanced", "Token0 Heavy", "Token1 Heavy"]:
        scenario_data = results_df[results_df['scenario'] == scenario]
        
        print(f"\n🔍 {scenario} Scenario:")
        print(f"   AS Model - Range A: {scenario_data['as_range_a'].mean():.2f}% ± {scenario_data['as_range_a'].std():.2f}%")
        print(f"   AS Model - Range B: {scenario_data['as_range_b'].mean():.2f}% ± {scenario_data['as_range_b'].std():.2f}%")
        print(f"   GLFT Model - Range A: {scenario_data['glft_range_a'].mean():.2f}% ± {scenario_data['glft_range_a'].std():.2f}%")
        print(f"   GLFT Model - Range B: {scenario_data['glft_range_b'].mean():.2f}% ± {scenario_data['glft_range_b'].std():.2f}%")
        
        # Calculate differences
        diff_a = scenario_data['glft_range_a'] - scenario_data['as_range_a']
        diff_b = scenario_data['glft_range_b'] - scenario_data['as_range_b']
        
        print(f"   Difference - Range A: {diff_a.mean():+.2f}% ± {diff_a.std():.2f}%")
        print(f"   Difference - Range B: {diff_b.mean():+.2f}% ± {diff_b.std():.2f}%")
    
    print()
    
    # Key insights
    print("🎯 Key Insights:")
    print("-" * 30)
    
    # Check if GLFT is more conservative
    balanced_data = results_df[results_df['scenario'] == 'Balanced']
    glft_more_conservative = (balanced_data['glft_range_a'].mean() < balanced_data['as_range_a'].mean() or 
                             balanced_data['glft_range_b'].mean() < balanced_data['as_range_b'].mean())
    
    if glft_more_conservative:
        print("✅ GLFT model is more conservative (smaller ranges)")
    else:
        print("⚠️  GLFT model is not more conservative")
    
    # Check volatility sensitivity
    volatility_correlation_as = balanced_data['as_range_a'].corr(balanced_data['volatility'])
    volatility_correlation_glft = balanced_data['glft_range_a'].corr(balanced_data['volatility'])
    
    print(f"📊 Volatility Sensitivity:")
    print(f"   AS Model: {volatility_correlation_as:.3f}")
    print(f"   GLFT Model: {volatility_correlation_glft:.3f}")
    
    if abs(volatility_correlation_glft) > abs(volatility_correlation_as):
        print("   ✅ GLFT is more sensitive to volatility")
    else:
        print("   ⚠️  AS is more sensitive to volatility")
    
    # Save detailed results
    results_df.to_csv('model_comparison_detailed.csv', index=False)
    print(f"\n💾 Detailed results saved to model_comparison_detailed.csv")
    
    # Create a simple visualization
    try:
        import matplotlib.pyplot as plt
        
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle('AS vs GLFT Model Comparison', fontsize=16)
        
        # Plot 1: Range A over time (Balanced scenario)
        balanced_data = results_df[results_df['scenario'] == 'Balanced']
        axes[0, 0].plot(balanced_data['timestamp'], balanced_data['as_range_a'], label='AS Model', alpha=0.7)
        axes[0, 0].plot(balanced_data['timestamp'], balanced_data['glft_range_a'], label='GLFT Model', alpha=0.7)
        axes[0, 0].set_title('Range A - Balanced Scenario')
        axes[0, 0].set_ylabel('Range (%)')
        axes[0, 0].legend()
        axes[0, 0].tick_params(axis='x', rotation=45)
        
        # Plot 2: Range B over time (Balanced scenario)
        axes[0, 1].plot(balanced_data['timestamp'], balanced_data['as_range_b'], label='AS Model', alpha=0.7)
        axes[0, 1].plot(balanced_data['timestamp'], balanced_data['glft_range_b'], label='GLFT Model', alpha=0.7)
        axes[0, 1].set_title('Range B - Balanced Scenario')
        axes[0, 1].set_ylabel('Range (%)')
        axes[0, 1].legend()
        axes[0, 1].tick_params(axis='x', rotation=45)
        
        # Plot 3: Range A by scenario
        scenarios = ['Balanced', 'Token0 Heavy', 'Token1 Heavy']
        as_means = [results_df[results_df['scenario'] == s]['as_range_a'].mean() for s in scenarios]
        glft_means = [results_df[results_df['scenario'] == s]['glft_range_a'].mean() for s in scenarios]
        
        x = np.arange(len(scenarios))
        width = 0.35
        
        axes[1, 0].bar(x - width/2, as_means, width, label='AS Model', alpha=0.7)
        axes[1, 0].bar(x + width/2, glft_means, width, label='GLFT Model', alpha=0.7)
        axes[1, 0].set_title('Average Range A by Scenario')
        axes[1, 0].set_ylabel('Range (%)')
        axes[1, 0].set_xticks(x)
        axes[1, 0].set_xticklabels(scenarios)
        axes[1, 0].legend()
        
        # Plot 4: Range B by scenario
        as_means_b = [results_df[results_df['scenario'] == s]['as_range_b'].mean() for s in scenarios]
        glft_means_b = [results_df[results_df['scenario'] == s]['glft_range_b'].mean() for s in scenarios]
        
        axes[1, 1].bar(x - width/2, as_means_b, width, label='AS Model', alpha=0.7)
        axes[1, 1].bar(x + width/2, glft_means_b, width, label='GLFT Model', alpha=0.7)
        axes[1, 1].set_title('Average Range B by Scenario')
        axes[1, 1].set_ylabel('Range (%)')
        axes[1, 1].set_xticks(x)
        axes[1, 1].set_xticklabels(scenarios)
        axes[1, 1].legend()
        
        plt.tight_layout()
        plt.savefig('model_comparison.png', dpi=300, bbox_inches='tight')
        print(f"📊 Visualization saved to model_comparison.png")
        
    except ImportError:
        print("⚠️  Matplotlib not available, skipping visualization")
    
    print("\n" + "=" * 50)
    print("✅ Detailed analysis completed!")

if __name__ == "__main__":
    analyze_models_over_time()
