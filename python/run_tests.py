#!/usr/bin/env python3
"""
Comprehensive test runner for AsymmetricLP Python implementation.
Runs unit tests, integration tests, and generates coverage reports.
"""
import os
import sys
import subprocess
import argparse
from pathlib import Path


def run_command(cmd, description):
    """Run a command and handle errors."""
    print(f"\n{'='*60}")
    print(f"🧪 {description}")
    print(f"{'='*60}")
    print(f"Running: {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Command failed with exit code {e.returncode}")
        print("STDOUT:", e.stdout)
        print("STDERR:", e.stderr)
        return False


def install_dependencies():
    """Install test dependencies."""
    print("📦 Installing test dependencies...")
    cmd = [sys.executable, "-m", "pip", "install", "-r", "requirements.txt"]
    return run_command(cmd, "Installing Dependencies")


def run_unit_tests():
    """Run unit tests."""
    cmd = [
        sys.executable, "-m", "pytest",
        "tests/test_strategy.py",
        "tests/test_models.py",
        "-v",
        "--tb=short",
        "-m", "unit"
    ]
    return run_command(cmd, "Running Unit Tests")


def run_integration_tests():
    """Run integration tests."""
    cmd = [
        sys.executable, "-m", "pytest",
        "tests/test_integration.py",
        "-v",
        "--tb=short",
        "-m", "integration"
    ]
    return run_command(cmd, "Running Integration Tests")


def run_all_tests():
    """Run all tests."""
    cmd = [
        sys.executable, "-m", "pytest",
        "tests/",
        "-v",
        "--tb=short"
    ]
    return run_command(cmd, "Running All Tests")


def run_coverage_report():
    """Generate coverage report."""
    cmd = [
        sys.executable, "-m", "pytest",
        "tests/",
        "--cov=.",
        "--cov-report=html",
        "--cov-report=term-missing",
        "--cov-report=xml"
    ]
    return run_command(cmd, "Generating Coverage Report")


def run_performance_tests():
    """Run performance tests (if available)."""
    cmd = [
        sys.executable, "-m", "pytest",
        "tests/",
        "-v",
        "-m", "slow",
        "--durations=10"
    ]
    return run_command(cmd, "Running Performance Tests")


def lint_code():
    """Run code linting."""
    # Check if flake8 is available
    try:
        subprocess.run([sys.executable, "-m", "flake8", "--version"], 
                      check=True, capture_output=True)
        
        cmd = [
            sys.executable, "-m", "flake8",
            ".",
            "--exclude=tests,__pycache__,.git",
            "--max-line-length=100",
            "--ignore=E203,W503"
        ]
        return run_command(cmd, "Running Code Linting")
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("⚠️  flake8 not available, skipping linting")
        return True


def main():
    """Main test runner."""
    parser = argparse.ArgumentParser(description="AsymmetricLP Test Runner")
    parser.add_argument("--unit", action="store_true", help="Run unit tests only")
    parser.add_argument("--integration", action="store_true", help="Run integration tests only")
    parser.add_argument("--coverage", action="store_true", help="Generate coverage report")
    parser.add_argument("--performance", action="store_true", help="Run performance tests")
    parser.add_argument("--lint", action="store_true", help="Run code linting")
    parser.add_argument("--install", action="store_true", help="Install dependencies")
    parser.add_argument("--all", action="store_true", help="Run all tests and checks")
    
    args = parser.parse_args()
    
    # Change to Python directory
    python_dir = Path(__file__).parent
    os.chdir(python_dir)
    
    print("🚀 AsymmetricLP Python Test Runner")
    print(f"📁 Working directory: {os.getcwd()}")
    
    success = True
    
    # Install dependencies if requested
    if args.install or args.all:
        success &= install_dependencies()
    
    # Run specific test types
    if args.unit:
        success &= run_unit_tests()
    elif args.integration:
        success &= run_integration_tests()
    elif args.coverage:
        success &= run_coverage_report()
    elif args.performance:
        success &= run_performance_tests()
    elif args.lint:
        success &= lint_code()
    elif args.all:
        # Run everything
        success &= install_dependencies()
        success &= run_unit_tests()
        success &= run_integration_tests()
        success &= run_coverage_report()
        success &= lint_code()
    else:
        # Default: run all tests
        success &= run_all_tests()
    
    # Print summary
    print(f"\n{'='*60}")
    if success:
        print("✅ All tests completed successfully!")
    else:
        print("❌ Some tests failed!")
    print(f"{'='*60}")
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
