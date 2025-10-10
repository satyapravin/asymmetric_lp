#!/usr/bin/env python3

import pandas as pd

def simple_backtest():
    """Simple backtest without positions or rebalancing"""
    
    # Read the data
    df = pd.read_csv('eth_usdc_minimal.csv')
    
    # Initial balances
    balance_0 = 0.2  # ETH
    balance_1 = 20000.0  # USDC
    
    print(f"Initial balances: {balance_0} ETH, {balance_1} USDC")
    
    # Track portfolio values
    portfolio_values = []
    
    for i, row in df.iterrows():
        current_price = row['close']
        
        # Calculate portfolio value
        portfolio_value = (balance_0 * current_price) + balance_1
        portfolio_values.append(portfolio_value)
        
        if i == 0:
            print(f"Initial price: ${current_price:.2f}")
            print(f"Initial portfolio value: ${portfolio_value:.2f}")
        elif i == len(df) - 1:
            print(f"Final price: ${current_price:.2f}")
            print(f"Final portfolio value: ${portfolio_value:.2f}")
    
    # Calculate return
    initial_value = portfolio_values[0]
    final_value = portfolio_values[-1]
    total_return = ((final_value - initial_value) / initial_value) * 100
    
    print(f"\nTotal return: {total_return:.2f}%")
    print(f"Expected return: ~0.37% (just price change)")

if __name__ == "__main__":
    simple_backtest()
