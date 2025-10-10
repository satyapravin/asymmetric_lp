#!/usr/bin/env python3
"""
Generate sample OHLC data for backtesting comparison.
"""
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
import random

def generate_sample_ohlc_data():
    """Generate 7 days of realistic BTC OHLC data."""
    # Set random seed for reproducibility
    np.random.seed(42)
    random.seed(42)
    
    # Generate 7 days of minute-by-minute OHLC data
    start_date = datetime(2024, 1, 1, 0, 0)
    end_date = start_date + timedelta(days=7)
    dates = pd.date_range(start=start_date, end=end_date, freq='1min')
    
    # Generate realistic BTC price data
    initial_price = 50000.0
    prices = [initial_price]
    volumes = []
    
    for i in range(1, len(dates)):
        # Random walk with some mean reversion
        change = np.random.normal(0, 0.001)  # 0.1% volatility per minute
        if i > 100:  # Add some mean reversion after initial period
            mean_reversion = -0.0001 * (prices[-1] - initial_price) / initial_price
            change += mean_reversion
        
        new_price = prices[-1] * (1 + change)
        prices.append(new_price)
        
        # Generate volume (higher during volatile periods)
        base_volume = 100
        volatility_multiplier = abs(change) * 1000
        volume = base_volume + volatility_multiplier + np.random.exponential(50)
        volumes.append(volume)
    
    # Create OHLC data
    ohlc_data = []
    for i in range(len(dates)):
        price = prices[i]
        
        # Generate realistic OHLC from price
        if i == 0:
            open_price = price
        else:
            open_price = prices[i-1]
        
        # Add some intraday volatility
        high_offset = np.random.uniform(0, 0.002)  # Up to 0.2% above open
        low_offset = np.random.uniform(0, 0.002)   # Up to 0.2% below open
        
        high_price = open_price * (1 + high_offset)
        low_price = open_price * (1 - low_offset)
        close_price = price
        
        # Ensure OHLC relationships are valid
        high_price = max(high_price, open_price, close_price)
        low_price = min(low_price, open_price, close_price)
        
        volume = volumes[i] if i < len(volumes) else 100
        
        ohlc_data.append({
            'timestamp': dates[i],
            'open': round(open_price, 2),
            'high': round(high_price, 2),
            'low': round(low_price, 2),
            'close': round(close_price, 2),
            'volume': round(volume, 1)
        })
    
    # Create DataFrame and save
    df = pd.DataFrame(ohlc_data)
    df.to_csv('btc_sample_7days.csv', index=False)
    
    print(f'Generated {len(df)} OHLC records from {start_date.strftime("%Y-%m-%d")} to {end_date.strftime("%Y-%m-%d")}')
    print(f'Price range: ${df["low"].min():.2f} - ${df["high"].max():.2f}')
    print(f'Average volume: {df["volume"].mean():.1f}')
    print('Sample data:')
    print(df.head())
    print('...')
    print(df.tail())
    
    return df

if __name__ == "__main__":
    generate_sample_ohlc_data()
