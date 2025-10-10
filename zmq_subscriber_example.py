#!/usr/bin/env python3
"""
AsymmetricLP - 0MQ Inventory Subscriber Example
Demonstrates how to receive inventory data from AsymmetricLP
"""
import json
import zmq
import time
from datetime import datetime

def main():
    """Main subscriber loop"""
    # Create 0MQ context and socket
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    
    # Connect to publisher
    socket.connect("tcp://localhost:5555")
    
    # Subscribe to all topics
    socket.setsockopt(zmq.SUBSCRIBE, b"")
    
    print("🔗 Connected to AsymmetricLP inventory publisher")
    print("📡 Listening for inventory updates...")
    print("=" * 80)
    
    try:
        while True:
            # Receive message
            message = socket.recv_string()
            
            # Parse topic and data
            topic, data_json = message.split(' ', 1)
            data = json.loads(data_json)
            
            # Print based on topic
            if topic == "inventory_update":
                print_inventory_update(data)
            elif topic == "rebalance_event":
                print_rebalance_event(data)
            elif topic == "error_event":
                print_error_event(data)
            else:
                print(f"📨 Unknown topic: {topic}")
                print(f"Data: {json.dumps(data, indent=2)}")
            
            print("-" * 80)
            
    except KeyboardInterrupt:
        print("\n👋 Disconnecting...")
    finally:
        socket.close()
        context.term()

def print_inventory_update(data):
    """Print inventory update data"""
    timestamp = data['timestamp']
    tokens = data['tokens']
    inventory = data['inventory']
    notionals = data['notionals']
    
    print(f"📊 Inventory Update - {timestamp}")
    print(f"   Pair: {tokens['token_a']['symbol']}/{tokens['token_b']['symbol']}")
    print(f"   Chain: {data['chain']} (ID: {data['chain_id']})")
    print()
    print(f"💰 Token Balances:")
    print(f"   {tokens['token_a']['symbol']}: {tokens['token_a']['balance']:.6f} "
          f"(${tokens['token_a']['usd_value']:.2f})")
    print(f"   {tokens['token_b']['symbol']}: {tokens['token_b']['balance']:.6f} "
          f"(${tokens['token_b']['usd_value']:.2f})")
    print()
    print(f"📈 Inventory Metrics:")
    print(f"   Current Ratio: {inventory['current_ratio']:.3f}")
    print(f"   Target Ratio: {inventory['target_ratio']:.3f}")
    print(f"   Deviation: {inventory['deviation']:.3f}")
    print(f"   Total Value: ${inventory['total_usd_value']:.2f}")
    print(f"   Should Rebalance: {inventory['should_rebalance']}")
    print()
    print(f"💵 Notionals for CeFi MM:")
    print(f"   Token A Notional: ${notionals['token_a_notional']:.2f}")
    print(f"   Token B Notional: ${notionals['token_b_notional']:.2f}")
    print(f"   Total Notional: ${notionals['total_notional']:.2f}")
    print(f"   Imbalance USD: ${notionals['imbalance_usd']:.2f}")
    print(f"   Imbalance %: {notionals['imbalance_percentage']:.2f}%")
    print()
    print(f"📊 Market Data:")
    print(f"   Spot Price: {data['market']['spot_price']:.6f}")
    print(f"   Volatility: {data['market']['volatility']:.4f}")
    print(f"   Range A: {tokens['token_a']['range_percentage']:.2f}%")
    print(f"   Range B: {tokens['token_b']['range_percentage']:.2f}%")

def print_rebalance_event(data):
    """Print rebalance event data"""
    timestamp = data['timestamp']
    tokens = data['tokens']
    rebalance = data['rebalance']
    
    print(f"🔄 Rebalance Event - {timestamp}")
    print(f"   Pair: {tokens['token_a_symbol']}/{tokens['token_b_symbol']}")
    print(f"   Chain: {data['chain']} (ID: {data['chain_id']})")
    print()
    print(f"📊 Rebalance Details:")
    print(f"   Spot Price: {rebalance['spot_price']:.6f}")
    print(f"   Old Ratio: {rebalance['old_inventory_ratio']:.3f}")
    print(f"   New Ratio: {rebalance['new_inventory_ratio']:.3f}")
    print(f"   Ratio Change: {rebalance['ratio_change']:.3f}")
    print(f"   Gas Used: {rebalance['gas_used']:,}")
    print()
    if data['fees']:
        print(f"💰 Fees Collected:")
        for token, amount in data['fees'].items():
            print(f"   {token}: {amount:.6f}")
    print()
    print(f"📊 New Ranges:")
    for token, range_pct in data['ranges'].items():
        print(f"   {token}: {range_pct:.2f}%")

def print_error_event(data):
    """Print error event data"""
    timestamp = data['timestamp']
    error = data['error']
    
    print(f"🚨 Error Event - {timestamp}")
    print(f"   Chain: {data['chain']} (ID: {data['chain_id']})")
    print()
    print(f"❌ Error Details:")
    print(f"   Type: {error['type']}")
    print(f"   Message: {error['message']}")
    print()
    if error['context']:
        print(f"📋 Context:")
        for key, value in error['context'].items():
            print(f"   {key}: {value}")

if __name__ == "__main__":
    main()
