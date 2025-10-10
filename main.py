#!/usr/bin/env python3
"""
Main application for AsymmetricLP bot.
Handles both live trading and backtesting modes.
"""
import logging
import signal
import sys
import time
import argparse
from datetime import datetime
from config import Config
from automated_rebalancer import AutomatedRebalancer
from backtest_engine import BacktestEngine
from utils import Logger

# Set up logging
Logger.setup_logging(level="INFO", log_file="logs/rebalancer.log")
logger = logging.getLogger(__name__)

def generate_sample_ohlc_data(start_date: str, end_date: str, output_file: str):
    """
    Generate minimal sample OHLC data for verification only
    
    Args:
        start_date: Start date in YYYY-MM-DD format
        end_date: End date in YYYY-MM-DD format
        output_file: Output CSV file path
    """
    import pandas as pd
    import numpy as np
    from datetime import datetime, timedelta
    
    # Parse dates
    start = datetime.strptime(start_date, '%Y-%m-%d')
    end = datetime.strptime(end_date, '%Y-%m-%d')
    
    # Generate minute-by-minute data
    timestamps = []
    current_time = start
    
    while current_time <= end:
        timestamps.append(current_time)
        current_time += timedelta(minutes=1)
    
    # Generate simple price data
    np.random.seed(42)  # For reproducible results
    base_price = 50000.0
    prices = [base_price]
    
    for i in range(1, len(timestamps)):
        # Random walk with slight volatility
        change = np.random.normal(0, 0.001)  # 0.1% volatility per minute
        new_price = prices[-1] * (1 + change)
        prices.append(max(new_price, 1000))  # Minimum price
    
    # Generate OHLC data
    data = []
    for i, (timestamp, close_price) in enumerate(zip(timestamps, prices)):
        if i == 0:
            open_price = close_price
        else:
            open_price = prices[i-1]
        
        # Simple OHLC generation
        volatility = 0.002  # 0.2% intraday volatility
        high_price = max(open_price, close_price) * (1 + np.random.uniform(0, volatility))
        low_price = min(open_price, close_price) * (1 - np.random.uniform(0, volatility))
        volume = np.random.uniform(10, 100)
        
        data.append({
            'timestamp': timestamp,
            'open': round(open_price, 2),
            'high': round(high_price, 2),
            'low': round(low_price, 2),
            'close': round(close_price, 2),
            'volume': round(volume, 2)
        })
    
    # Save to CSV
    df = pd.DataFrame(data)
    df.to_csv(output_file, index=False)
    
    print(f"Generated {len(df)} OHLC records from {start_date} to {end_date}")

class RebalancerApp:
    """Main application class for the automated rebalancer"""
    
    def __init__(self):
        """Initialize the application"""
        self.config = Config()
        
        # Validate configuration
        try:
            self.config.validate_config()
            self.config.validate_token_addresses()
            logger.info("✅ Configuration validation passed")
        except ValueError as e:
            logger.error(f"❌ Configuration validation failed: {e}")
            sys.exit(1)
        
        self.rebalancer = None
        self.running = False
        
        # Set up signal handlers for graceful shutdown
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
        
        logger.info("RebalancerApp initialized")
    
    def signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        logger.info(f"Received signal {signum}, shutting down gracefully...")
        
        # Send shutdown notification
        if self.rebalancer and self.rebalancer.alert_manager:
            try:
                signal_name = "SIGINT" if signum == 2 else "SIGTERM" if signum == 15 else f"Signal {signum}"
                self.rebalancer.alert_manager.send_shutdown_notification(f"Received {signal_name}")
            except Exception as alert_error:
                logger.warning(f"Failed to send shutdown notification: {alert_error}")
        
        self.stop()
    
    def start(self, token0: str, token1: str, fee: int):
        """
        Start the automated rebalancer
        
        Args:
            token0: Token0 address
            token1: Token1 address
            fee: Fee tier
        """
        try:
            logger.info("Starting Automated Uniswap V3 LP Rebalancer")
            logger.info(f"Token Pair: {token0}/{token1}")
            logger.info(f"Fee Tier: {fee}")
            logger.info(f"Max Inventory Deviation: {self.config.MAX_INVENTORY_DEVIATION}")
            logger.info(f"Monitoring Interval: {self.config.MONITORING_INTERVAL_SECONDS} seconds")
            
            # Initialize rebalancer
            self.rebalancer = AutomatedRebalancer(self.config)
            
            # Send startup notification
            try:
                token_a_info = self.rebalancer.client.get_token_info(token0)
                token_b_info = self.rebalancer.client.get_token_info(token1)
                wallet_address = self.config.WALLET_ADDRESS
                
                self.rebalancer.alert_manager.send_startup_notification(
                    token_a_symbol=token_a_info['symbol'],
                    token_b_symbol=token_b_info['symbol'],
                    chain_name=self.config.CHAIN_NAME,
                    wallet_address=wallet_address
                )
            except Exception as alert_error:
                logger.warning(f"Failed to send startup notification: {alert_error}")
            
            # Initialize positions on startup
            logger.info("Initializing positions on startup...")
            if not self.rebalancer.initialize_positions():
                logger.error("Failed to initialize positions")
                return False
            
            # Perform initial rebalance if positions exist
            positions = self.rebalancer.get_position_ranges()
            if positions:
                logger.info("Performing initial rebalance...")
                initial_result = self.rebalancer.rebalance_positions(token0, token1, fee)
                
                if 'error' in initial_result:
                    logger.error(f"Initial rebalance failed: {initial_result['error']}")
                    return False
                
                logger.info("Initial rebalance completed successfully")
            else:
                logger.info("No existing positions found, will create new ones when monitoring starts")
            
            # Start monitoring
            self.rebalancer.start_monitoring(token0, token1, fee)
            self.running = True
            
            logger.info("Automated rebalancer is now running...")
            logger.info("Press Ctrl+C to stop")
            
            # Keep the main thread alive
            while self.running:
                time.sleep(1)
                
                # Log status every 5 minutes
                if int(time.time()) % 300 == 0:
                    status = self.rebalancer.get_status()
                    logger.info(f"Status: Running={status['is_running']}, "
                              f"Last Price={status['last_spot_price']}, "
                              f"Positions={status['current_positions']}")
            
            return True
            
        except Exception as e:
            logger.error(f"Error starting rebalancer: {e}")
            return False
    
    def stop(self):
        """Stop the automated rebalancer"""
        if self.rebalancer and self.running:
            logger.info("Stopping automated rebalancer...")
            self.rebalancer.stop_monitoring()
            self.running = False
            logger.info("Automated rebalancer stopped")
    
    def get_status(self) -> dict:
        """Get current status"""
        if self.rebalancer:
            return self.rebalancer.get_status()
        return {'is_running': False}

def run_historical_mode(args):
    """
    Run the application in historical backtesting mode
    
    Args:
        args: Command line arguments
    """
    print("📊 Running in Historical Backtesting Mode")
    print("=" * 50)
    
    # Initialize configuration
    config = Config()
    
    # Override some settings for backtesting if provided
    if args.fee_tier:
        config.FEE_TIER = args.fee_tier
    if args.target_ratio:
        config.TARGET_INVENTORY_RATIO = args.target_ratio
    if args.max_deviation:
        config.MAX_INVENTORY_DEVIATION = args.max_deviation
    
    print(f"⚙️  Configuration:")
    print(f"   Fee Tier: {config.FEE_TIER} bps ({config.FEE_TIER/10000:.1%})")
    print(f"   Target Inventory Ratio: {config.TARGET_INVENTORY_RATIO:.1%}")
    print(f"   Max Inventory Deviation: {config.MAX_INVENTORY_DEVIATION:.1%}")
    
    # Check if OHLC file exists
    import os
    if not os.path.exists(args.ohlc_file):
        if args.generate_sample:
            print(f"\n📊 Generating sample OHLC data for verification...")
            generate_sample_ohlc_data(
                start_date=args.start_date or "2024-01-01",
                end_date=args.end_date or "2024-01-07",
                output_file=args.ohlc_file
            )
            print(f"✅ Sample data generated: {args.ohlc_file}")
            print("⚠️  WARNING: Using generated sample data. For real backtesting, use actual OHLC data.")
        else:
            print(f"❌ OHLC file not found: {args.ohlc_file}")
            print("   Please provide a valid OHLC CSV file or use --generate-sample for verification.")
            return 1
    else:
        print(f"✅ Using OHLC file: {args.ohlc_file}")
    
    # Initialize backtest engine
    print(f"\n🔧 Initializing backtest engine...")
    engine = BacktestEngine(config)
    print("✅ Backtest engine initialized")
    
    # Parse dates
    start_date = None
    end_date = None
    
    if args.start_date:
        start_date = datetime.strptime(args.start_date, '%Y-%m-%d')
    if args.end_date:
        end_date = datetime.strptime(args.end_date, '%Y-%m-%d')
    
    # Run backtest
    print(f"\n📈 Running backtest...")
    print(f"   OHLC File: {args.ohlc_file}")
    print(f"   Initial Balance 0: {args.initial_balance_0}")
    print(f"   Initial Balance 1: {args.initial_balance_1}")
    print(f"   Start Date: {start_date}")
    print(f"   End Date: {end_date}")
    
    try:
        result = engine.run_backtest(
            ohlc_file=args.ohlc_file,
            initial_balance_0=args.initial_balance_0,
            initial_balance_1=args.initial_balance_1,
            start_date=start_date,
            end_date=end_date
        )
        
        print("✅ Backtest completed successfully!")
        
    except Exception as e:
        print(f"❌ Backtest failed: {e}")
        return 1
    
    # Display results
    print(f"\n🎯 Backtest Results:")
    print(f"   Period: {result.start_time.strftime('%Y-%m-%d')} to {result.end_time.strftime('%Y-%m-%d')}")
    print(f"   Duration: {(result.end_time - result.start_time).days} days")
    
    # Calculate performance metrics
    initial_value = result.initial_balance_0 + result.initial_balance_1 * 50000  # Assume $50k BTC
    final_value = result.final_balance_0 + result.final_balance_1 * 50000  # Assume $50k BTC
    total_return = (final_value - initial_value) / initial_value
    
    print(f"   Initial Value: ${initial_value:,.2f}")
    print(f"   Final Value: ${final_value:,.2f}")
    print(f"   Total Return: {total_return:.2%}")
    print(f"   Total Fees Collected: ${result.total_fees_collected:.2f}")
    print(f"   Total Rebalances: {result.total_rebalances}")
    print(f"   Total Trades Detected: {result.total_trades}")
    
    if result.total_rebalances > 0:
        avg_rebalance_interval = (result.end_time - result.start_time).total_seconds() / result.total_rebalances
        print(f"   Average Rebalance Interval: {avg_rebalance_interval/3600:.1f} hours")
    
    if result.total_trades > 0:
        avg_trades_per_day = result.total_trades / (result.end_time - result.start_time).days
        print(f"   Average Trades per Day: {avg_trades_per_day:.1f}")
    
    # Save results
    if args.output:
        print(f"\n💾 Saving results to {args.output}...")
        engine.save_results(result, args.output)
        print("✅ Results saved")
    
    # Display sample data
    if result.trades:
        print(f"\n📋 Sample Trades (first 5 of {len(result.trades)}):")
        for i, trade in enumerate(result.trades[:5]):
            print(f"   {trade.timestamp.strftime('%Y-%m-%d %H:%M')}: {trade.trade_type.upper()} at ${trade.price:,.2f}, volume: {trade.volume:.2f}")
    
    if result.rebalances:
        print(f"\n📋 Sample Rebalances (first 3 of {len(result.rebalances)}):")
        for i, rebalance in enumerate(result.rebalances[:3]):
            print(f"   {rebalance['timestamp'].strftime('%Y-%m-%d %H:%M')}: Price ${rebalance['price']:,.2f}, "
                  f"burned {rebalance['positions_burned']} positions, collected ${rebalance['fees_collected_0']:.2f} fees")
    
    return 0

def run_live_mode(args):
    """
    Run the application in live trading mode
    
    Args:
        args: Command line arguments
    """
    print("🚀 Running in Live Trading Mode")
    print("=" * 50)
    
    try:
        # Initialize configuration
        config = Config()
        
        # Get token pair from configuration
        token0 = config.TOKEN_A_ADDRESS
        token1 = config.TOKEN_B_ADDRESS
        fee = config.FEE_TIER  # Use fee tier from config
        
        # Detect chain
        chain_name = config.detect_chain_from_rpc()
        chain_config = config.get_chain_config(chain_name)
        
        # Validate configuration
        if not config.validate_config():
            print("❌ Configuration validation failed")
            sys.exit(1)
        
        print(f"🌐 Chain: {chain_config['name']} (ID: {chain_config['chain_id']})")
        print(f"📊 Token Pair: {token0}/{token1}")
        print(f"💰 Fee Tier: {fee} ({fee/10000}%)")
        print(f"⚡ Max Inventory Deviation: {config.MAX_INVENTORY_DEVIATION}")
        print(f"⏰ Monitoring Interval: {config.MONITORING_INTERVAL_SECONDS} seconds")
        print(f"🔗 RPC URL: {config.ETHEREUM_RPC_URL}")
        print(f"👛 Wallet: {config.PRIVATE_KEY[:10]}...")
        
        # Create and start the application
        app = RebalancerApp()
        
        # Start the rebalancer
        success = app.start(token0, token1, fee)
        
        if not success:
            print("❌ Failed to start rebalancer")
            sys.exit(1)
        
    except KeyboardInterrupt:
        print("\n🛑 Shutdown requested by user")
        if 'app' in locals():
            app.stop()
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        print(f"❌ Unexpected error: {e}")
        sys.exit(1)
    
    print("👋 Goodbye!")
    return 0

def main():
    """Main function with mode selection"""
    parser = argparse.ArgumentParser(
        description='AsymmetricLP - Uniswap V3 LP Rebalancer with Asymmetric Range Allocation',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Live trading mode (default)
  python main.py

  # Historical backtesting mode
  python main.py --historical-mode --ohlc-file btc_data.csv --start-date 2024-01-01 --end-date 2024-01-31

  # Test with realistic fee tiers
  python main.py --historical-mode --ohlc-file data.csv --fee-tier 50   # 0.05% (5 bps)
  python main.py --historical-mode --ohlc-file data.csv --fee-tier 300  # 0.3% (30 bps)

  # Custom backtest parameters
  python main.py --historical-mode --ohlc-file data.csv --initial-balance-0 10000 --initial-balance-1 0.2 --fee-tier 300
        """
    )
    
    # Mode selection
    parser.add_argument('--historical-mode', action='store_true',
                       help='Run in historical backtesting mode instead of live trading')
    
    # Historical mode arguments
    parser.add_argument('--ohlc-file', default='btc_ohlc_sample.csv',
                       help='Path to OHLC CSV file (default: btc_ohlc_sample.csv)')
    parser.add_argument('--start-date', 
                       help='Start date for backtest (YYYY-MM-DD)')
    parser.add_argument('--end-date',
                       help='End date for backtest (YYYY-MM-DD)')
    parser.add_argument('--initial-balance-0', type=float, default=5000.0,
                       help='Initial token A balance (default: 5000.0)')
    parser.add_argument('--initial-balance-1', type=float, default=0.1,
                       help='Initial token B balance (default: 0.1)')
    parser.add_argument('--output', default='backtest_results.json',
                       help='Output file for backtest results (default: backtest_results.json)')
    
    # Backtest configuration overrides
    parser.add_argument('--fee-tier', type=int,
                       help='Fee tier in basis points (overrides config)')
    parser.add_argument('--target-ratio', type=float,
                       help='Target inventory ratio (overrides config)')
    parser.add_argument('--max-deviation', type=float,
                       help='Maximum inventory deviation (overrides config)')
    
    # Sample data generation (for verification only)
    parser.add_argument('--generate-sample', action='store_true',
                       help='Generate sample OHLC data for verification (not recommended for real backtesting)')
    
    args = parser.parse_args()
    
    print("🤖 AsymmetricLP")
    print("=" * 50)
    
    try:
        if args.historical_mode:
            return run_historical_mode(args)
        else:
            return run_live_mode(args)
            
    except KeyboardInterrupt:
        print("\n🛑 Shutdown requested by user")
        return 1
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        print(f"❌ Unexpected error: {e}")
        return 1

if __name__ == "__main__":
    main()
