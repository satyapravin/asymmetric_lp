#!/usr/bin/env python3

import pandas as pd

# Read the data
df = pd.read_csv('eth_usdc_minimal.csv')
initial_price = df['close'].iloc[0]

print(f'Initial price: ${initial_price:.2f}')
print(f'Initial balance 0: 0.2 ETH')
print(f'Initial balance 1: 20000 USDC')
print(f'Initial value 0: 0.2 * {initial_price:.2f} = ${0.2 * initial_price:.2f}')
print(f'Initial value 1: $20000.00')
print(f'Total initial value: ${0.2 * initial_price + 20000:.2f}')

# Check final price
final_price = df['close'].iloc[-1]
print(f'\nFinal price: ${final_price:.2f}')
print(f'Price change: {((final_price - initial_price) / initial_price) * 100:.2f}%')
