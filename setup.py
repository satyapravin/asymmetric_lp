#!/usr/bin/env python3
"""
AsymmetricLP - Setup Script
Automated setup and installation for AsymmetricLP
"""
import os
import sys
import subprocess
import shutil
from pathlib import Path

def check_python_version():
    """Check if Python version is 3.8 or higher"""
    if sys.version_info < (3, 8):
        print("❌ Python 3.8 or higher is required")
        print(f"   Current version: {sys.version}")
        sys.exit(1)
    print(f"✅ Python version: {sys.version.split()[0]}")

def install_dependencies():
    """Install required dependencies"""
    print("📦 Installing dependencies...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", "requirements.txt"])
        print("✅ Dependencies installed successfully")
    except subprocess.CalledProcessError as e:
        print(f"❌ Failed to install dependencies: {e}")
        sys.exit(1)

def create_env_file():
    """Create .env file from template"""
    if os.path.exists('.env'):
        print("⚠️  .env file already exists, skipping creation")
        return
    
    if os.path.exists('env.example'):
        shutil.copy('env.example', '.env')
        print("✅ Created .env file from template")
        print("   Please edit .env with your actual configuration values")
    else:
        print("❌ env.example not found")

def create_directories():
    """Create necessary directories"""
    directories = ['logs']
    
    for directory in directories:
        Path(directory).mkdir(exist_ok=True)
        print(f"✅ Created directory: {directory}")

def run_basic_tests():
    """Run basic functionality tests"""
    print("🧪 Running basic tests...")
    try:
        # Test imports
        from config import Config
        from automated_rebalancer import AutomatedRebalancer
        from inventory_model import InventoryModel
        from alert_manager import TelegramAlertManager
        from inventory_publisher import InventoryPublisher
        from backtest_engine import BacktestEngine
        
        print("✅ All modules import successfully")
        
        # Test config initialization
        config = Config()
        print("✅ Configuration loads successfully")
        
    except Exception as e:
        print(f"⚠️  Basic tests failed: {e}")
        print("⚠️  You can still use the project, but some features may not work correctly")

def main():
    """Main setup function"""
    print("🚀 Setting up AsymmetricLP")
    print("=" * 50)
    
    # Check Python version
    check_python_version()
    
    # Install dependencies
    install_dependencies()
    
    # Create .env file
    create_env_file()
    
    # Create directories
    create_directories()
    
    # Run tests
    run_basic_tests()
    
    print("\n" + "=" * 50)
    print("🎉 Setup completed successfully!")
    print("\nNext steps:")
    print("1. Edit .env file with your actual values:")
    print("   - ETHEREUM_RPC_URL: Your Ethereum RPC endpoint")
    print("   - PRIVATE_KEY: Reference to your private key environment variable")
    print("   - Token addresses for the pairs you want to trade")
    print("   - Copy chain configuration from chain.conf")
    print("\n2. Test the system:")
    print("   python main.py --help")
    print("\n3. Run backtesting:")
    print("   python main.py --historical-mode --ohlc-file your_data.csv")
    print("\n4. Start live trading:")
    print("   python main.py")
    print("\n⚠️  Security Warning:")
    print("   - Never commit your private key to version control")
    print("   - Use testnets for initial testing")
    print("   - Start with small amounts")
    print("   - Monitor gas costs and adjust limits as needed")

if __name__ == "__main__":
    main()