"""
Core rebalancing logic for AsymmetricLP.
Monitors positions and rebalances based on inventory imbalance.
"""
import logging
import time
import threading
from typing import Dict, Any, List, Optional, Tuple
from web3 import Web3
from config import Config
from uniswap_client import UniswapV3Client
from lp_position_manager import LPPositionManager
from utils import UniswapV3Utils, ErrorHandler, Logger, retry_on_failure, is_retryable_error
from models.model_factory import ModelFactory
from alert_manager import TelegramAlertManager
from inventory_publisher import InventoryPublisher

logger = logging.getLogger(__name__)

class AutomatedRebalancer:
    """Automated LP position rebalancer"""
    
    def __init__(self, config: Config = None):
        """Initialize the automated rebalancer"""
        self.config = config or Config()
        self.config.validate_config()
        
        # Initialize clients
        self.client = UniswapV3Client(self.config)
        self.lp_manager = LPPositionManager(self.client)
        self.utils = UniswapV3Utils()
        
        # Initialize alert manager
        self.alert_manager = TelegramAlertManager(self.config)
        
        # Initialize inventory publisher
        self.inventory_publisher = InventoryPublisher(self.config)
        
        # Initialize inventory model (can be easily swapped)
        model_name = getattr(self.config, 'INVENTORY_MODEL', 'AvellanedaStoikovModel')
        self.inventory_model = ModelFactory.create_model(model_name, self.config)
        logger.info(f"Using inventory model: {model_name}")
        
        # Monitoring state
        self.is_running = False
        self.monitoring_thread = None
        self.current_positions = []
        self.last_rebalance_time = 0
        
        # Price tracking
        self.last_spot_price = None
        self.price_history = []
        
        logger.info("Automated Rebalancer initialized")
    
    def validate_token_ordering(self, token_a: str, token_b: str, fee: int) -> bool:
        """
        Validate that our token ordering matches the pool's token0/token1 ordering
        
        Args:
            token_a: Our configured token A address
            token_b: Our configured token B address  
            fee: Fee tier
            
        Returns:
            True if ordering is correct, False otherwise
        """
        try:
            # Get pool information to check actual token ordering
            pool_address = self.client.get_pool_address(token_a, token_b, fee)
            pool_info = self.client.get_pool_info(pool_address)
            
            pool_token0 = pool_info['token0']
            pool_token1 = pool_info['token1']
            
            # Get token information for better logging
            token0_info = self.client.get_token_info(pool_token0)
            token1_info = self.client.get_token_info(pool_token1)
            
            logger.info("Pool Token Ordering Validation:")
            logger.info(f"  Pool Address: {pool_address}")
            logger.info(f"  Token0: {pool_token0} ({token0_info['symbol']})")
            logger.info(f"  Token1: {pool_token1} ({token1_info['symbol']})")
            logger.info(f"  Our Token A: {token_a}")
            logger.info(f"  Our Token B: {token_b}")
            
            # Check if our token ordering matches pool ordering
            token_a_is_token0 = token_a.lower() == pool_token0.lower()
            token_b_is_token1 = token_b.lower() == pool_token1.lower()
            
            if token_a_is_token0 and token_b_is_token1:
                logger.info("✅ Token ordering validation PASSED")
                logger.info(f"  Token A ({token0_info['symbol']}) = Token0")
                logger.info(f"  Token B ({token1_info['symbol']}) = Token1")
                return True
            else:
                logger.error("❌ Token ordering validation FAILED")
                logger.error(f"  Expected: Token A = Token0, Token B = Token1")
                logger.error(f"  Actual: Token A = {'Token0' if token_a_is_token0 else 'Token1'}")
                logger.error(f"  Actual: Token B = {'Token0' if not token_b_is_token1 else 'Token1'}")
                logger.error("")
                logger.error("SOLUTION:")
                logger.error(f"  Swap TOKEN_A_ADDRESS and TOKEN_B_ADDRESS in your .env file")
                logger.error(f"  TOKEN_A_ADDRESS={pool_token0}")
                logger.error(f"  TOKEN_B_ADDRESS={pool_token1}")
                return False
                
        except Exception as e:
            logger.error(f"Error validating token ordering: {e}")
            logger.error("Cannot validate token ordering - proceeding with risk")
            return False
    
    def get_wallet_balances(self, token0: str, token1: str) -> Tuple[int, int]:
        """
        Get wallet balances for both tokens
        
        Args:
            token0: Token0 address
            token1: Token1 address
            
        Returns:
            Tuple of (token0_balance, token1_balance) in wei
        """
        try:
            # Get token balances using dynamic decimals
            token0_balance = self._get_token_balance(token0)
            token1_balance = self._get_token_balance(token1)
            
            return token0_balance, token1_balance
            
        except Exception as e:
            logger.error(f"Error getting wallet balances: {e}")
            return 0, 0
    
    def _get_token_balance(self, token_address: str) -> int:
        """
        Get token balance with dynamic decimal handling
        
        Args:
            token_address: Token contract address
            
        Returns:
            Token balance in wei (raw units)
        """
        try:
            # Check if it's ETH/WETH
            if token_address.lower() == self.config.WETH_ADDRESS.lower():
                return self.client.w3.eth.get_balance(self.client.wallet_address)
            
            # For ERC20 tokens, get balance using dynamic decimals
            token_contract = self.client.w3.eth.contract(
                address=token_address,
                abi=self.client.erc20_abi
            )
            
            balance = token_contract.functions.balanceOf(self.client.wallet_address).call()
            return balance
            
        except Exception as e:
            logger.error(f"Error getting balance for token {token_address}: {e}")
            return 0
    
    def _get_erc20_balance(self, token_address: str) -> int:
        """Get ERC20 token balance"""
        try:
            # ERC20 balanceOf ABI
            erc20_abi = [
                {
                    "inputs": [{"internalType": "address", "name": "account", "type": "address"}],
                    "name": "balanceOf",
                    "outputs": [{"internalType": "uint256", "name": "", "type": "uint256"}],
                    "stateMutability": "view",
                    "type": "function"
                }
            ]
            
            token_contract = self.client.w3.eth.contract(
                address=token_address,
                abi=erc20_abi
            )
            
            balance = token_contract.functions.balanceOf(self.client.wallet_address).call()
            return balance
            
        except Exception as e:
            logger.error(f"Error getting ERC20 balance for {token_address}: {e}")
            return 0
    
    @retry_on_failure(max_retries=3, delay=1.0)
    def get_current_spot_price(self, token0: str, token1: str, fee: int) -> float:
        """
        Get current spot price from the pool
        
        Args:
            token0: Token0 address (lower address in Uniswap ordering)
            token1: Token1 address (higher address in Uniswap ordering)
            fee: Fee tier
            
        Returns:
            Current spot price (token1 per token0)
        """
        try:
            pool_address = self.client.get_pool_address(token0, token1, fee)
            pool_info = self.client.get_pool_info(pool_address)
            
            # Get actual token addresses from pool
            pool_token0 = pool_info['token0']
            pool_token1 = pool_info['token1']
            
            # Get token decimals dynamically
            token0_decimals = self.client.get_token_decimals(pool_token0)
            token1_decimals = self.client.get_token_decimals(pool_token1)
            
            # Convert sqrtPriceX96 to actual price
            sqrt_price_x96 = pool_info['sqrt_price_x96']
            price = (sqrt_price_x96 / (2 ** 96)) ** 2
            
            # Adjust for token decimals difference
            # sqrtPriceX96 represents sqrt(price) where price = token1/token0
            price_adjusted = price * (10 ** (token0_decimals - token1_decimals))
            
            logger.debug(f"Spot price calculation:")
            logger.debug(f"  Pool: {pool_token0}/{pool_token1}")
            logger.debug(f"  Decimals: {token0_decimals}/{token1_decimals}")
            logger.debug(f"  Raw price: {price:.10f}")
            logger.debug(f"  Adjusted price: {price_adjusted:.10f}")
            
            return price_adjusted
            
        except Exception as e:
            logger.error(f"Error getting spot price: {e}")
            return 0.0
    
    def _get_token_balance(self, token_address: str) -> float:
        """
        Get token balance for a given token address
        
        Args:
            token_address: Token contract address
            
        Returns:
            Token balance in raw units
        """
        try:
            if token_address.lower() == self.config.WETH_ADDRESS.lower():
                # Get ETH balance
                balance_wei = self.client.w3.eth.get_balance(self.client.wallet_address)
                return balance_wei
            else:
                # Get ERC20 token balance
                token_contract = self.client.w3.eth.contract(
                    address=token_address,
                    abi=self.client.erc20_abi
                )
                balance = token_contract.functions.balanceOf(self.client.wallet_address).call()
                return balance
                
        except Exception as e:
            logger.error(f"Error getting token balance for {token_address}: {e}")
            return 0
    
    @retry_on_failure(max_retries=3, delay=2.0)
    def initialize_positions(self) -> bool:
        """
        Initialize positions on startup/restart by querying the blockchain
        
        Returns:
            True if initialization successful
        """
        try:
            logger.info("Initializing positions on startup...")
            
            # Query all positions from blockchain
            positions = self._query_positions_from_blockchain()
            
            # Store in memory
            self.current_positions = positions
            
            logger.info(f"Initialized with {len(positions)} active positions")
            
            # Log position details
            for pos in positions:
                logger.info(f"  Position {pos['token_id']}: "
                          f"Range {pos['tick_lower']}-{pos['tick_upper']}, "
                          f"Liquidity {pos['liquidity']}, "
                          f"Fees owed: {pos['tokens_owed0']}/{pos['tokens_owed1']}")
            
            return True
            
        except Exception as e:
            logger.error(f"Error initializing positions: {e}")
            return False
    
    def _query_positions_from_blockchain(self) -> List[Dict[str, Any]]:
        """
        Query all positions from blockchain (only used on startup/restart)
        
        Returns:
            List of position information
        """
        positions = []
        
        try:
            # Get all NFT tokens owned by the wallet
            owned_tokens = self._get_owned_nft_tokens()
            
            for token_id in owned_tokens:
                try:
                    # Get position info for each token
                    position_info = self.client.get_position_info(token_id)
                    
                    # Only include positions with liquidity > 0
                    if position_info['liquidity'] > 0:
                        positions.append({
                            'token_id': token_id,
                            'token0': position_info['token0'],
                            'token1': position_info['token1'],
                            'fee': position_info['fee'],
                            'tick_lower': position_info['tick_lower'],
                            'tick_upper': position_info['tick_upper'],
                            'liquidity': position_info['liquidity'],
                            'tokens_owed0': position_info['tokens_owed0'],
                            'tokens_owed1': position_info['tokens_owed1']
                        })
                        
                except Exception as e:
                    logger.warning(f"Error getting info for position {token_id}: {e}")
                    continue
            
            logger.info(f"Found {len(positions)} active LP positions from blockchain")
            return positions
            
        except Exception as e:
            logger.error(f"Error querying positions from blockchain: {e}")
            return []
    
    def get_position_ranges(self) -> List[Dict[str, Any]]:
        """
        Get current LP position ranges from memory (not blockchain)
        
        Returns:
            List of position information with ranges
        """
        return self.current_positions.copy()
    
    def verify_positions_burned(self) -> bool:
        """
        Verify that all positions have been successfully burned after rebalancing
        
        Returns:
            True if no positions remain, False otherwise
        """
        try:
            logger.info("Verifying all positions have been burned...")
            
            # Query blockchain to check if any positions still exist
            remaining_positions = self._query_positions_from_blockchain()
            
            if len(remaining_positions) == 0:
                logger.info("✅ All positions successfully burned")
                return True
            else:
                logger.warning(f"⚠️ {len(remaining_positions)} positions still remain:")
                for pos in remaining_positions:
                    logger.warning(f"  Position {pos['token_id']} still exists")
                return False
                
        except Exception as e:
            logger.error(f"Error verifying positions burned: {e}")
            return False
    
    def _get_owned_nft_tokens(self) -> List[int]:
        """
        Get all NFT token IDs owned by the wallet from NonfungiblePositionManager
        
        Returns:
            List of token IDs
        """
        try:
            # ERC721 balanceOf and tokenOfOwnerByIndex ABI
            nft_abi = [
                {
                    "inputs": [{"internalType": "address", "name": "owner", "type": "address"}],
                    "name": "balanceOf",
                    "outputs": [{"internalType": "uint256", "name": "", "type": "uint256"}],
                    "stateMutability": "view",
                    "type": "function"
                },
                {
                    "inputs": [
                        {"internalType": "address", "name": "owner", "type": "address"},
                        {"internalType": "uint256", "name": "index", "type": "uint256"}
                    ],
                    "name": "tokenOfOwnerByIndex",
                    "outputs": [{"internalType": "uint256", "name": "", "type": "uint256"}],
                    "stateMutability": "view",
                    "type": "function"
                }
            ]
            
            # Create contract instance for NonfungiblePositionManager
            nft_contract = self.client.w3.eth.contract(
                address=self.config.UNISWAP_V3_POSITION_MANAGER,
                abi=nft_abi
            )
            
            # Get number of NFTs owned
            balance = nft_contract.functions.balanceOf(self.client.wallet_address).call()
            
            token_ids = []
            for i in range(balance):
                try:
                    token_id = nft_contract.functions.tokenOfOwnerByIndex(
                        self.client.wallet_address, i
                    ).call()
                    token_ids.append(token_id)
                except Exception as e:
                    logger.warning(f"Error getting token at index {i}: {e}")
                    continue
            
            logger.info(f"Wallet owns {len(token_ids)} NFT positions")
            return token_ids
            
        except Exception as e:
            logger.error(f"Error getting owned NFT tokens: {e}")
            return []
    
    
    def calculate_dynamic_ranges(self, token0_balance: int, token1_balance: int, 
                                spot_price: float) -> Tuple[float, float]:
        """
        Calculate dynamic ranges using inventory management model
        
        Args:
            token0_balance: Token0 balance in wei
            token1_balance: Token1 balance in wei
            spot_price: Current spot price
            
        Returns:
            Tuple of (range_a_percentage, range_b_percentage)
        """
        try:
            # Use inventory model to calculate optimal ranges
            inventory_result = self.inventory_model.calculate_lp_ranges(
                token0_balance, token1_balance, spot_price, self.price_history,
                self.config.TOKEN_A_ADDRESS, self.config.TOKEN_B_ADDRESS, self.client
            )
            
            return inventory_result['token_a_range_percent'], inventory_result['token_b_range_percent']
            
        except Exception as e:
            logger.error(f"Error calculating dynamic ranges with inventory model: {e}")
            # Fallback to simple calculation
            return self.config.MIN_RANGE_PERCENTAGE, self.config.MIN_RANGE_PERCENTAGE
    
    def should_rebalance(self, spot_price: float, positions: List[Dict[str, Any]]) -> bool:
        """
        Determine if rebalancing is needed based on price proximity and inventory imbalance
        
        Args:
            spot_price: Current spot price
            positions: Current LP positions
            
        Returns:
            True if rebalancing is needed
        """
        try:
            if not positions:
                return True  # No positions, need to create initial ones
            
            # Get current wallet balances
            token0_balance, token1_balance = self.get_wallet_balances(
                self.config.TOKEN_A_ADDRESS, self.config.TOKEN_B_ADDRESS
            )
            
            # Check inventory imbalance using inventory model
            inventory_result = self.inventory_model.calculate_lp_ranges(
                token0_balance, token1_balance, spot_price, self.price_history,
                self.config.TOKEN_A_ADDRESS, self.config.TOKEN_B_ADDRESS, self.client
            )
            
            # Rebalance if inventory is severely imbalanced
            if inventory_result['target_rebalance']:
                logger.info(f"Inventory imbalance {inventory_result['inventory_imbalance']:.3f} exceeds threshold")
                return True
            
            return False
            
        except Exception as e:
            logger.error(f"Error checking rebalance condition: {e}")
            return False
    
    def get_inventory_status(self, spot_price: float) -> Dict[str, Any]:
        """
        Get detailed inventory status for monitoring
        
        Args:
            spot_price: Current spot price
            
        Returns:
            Dictionary with inventory information
        """
        try:
            token0_balance, token1_balance = self.get_wallet_balances(
                self.config.TOKEN_A_ADDRESS, self.config.TOKEN_B_ADDRESS
            )
            
            inventory_result = self.inventory_model.calculate_lp_ranges(
                token0_balance, token1_balance, spot_price, self.price_history,
                self.config.TOKEN_A_ADDRESS, self.config.TOKEN_B_ADDRESS, self.client
            )
            
            return {
                'spot_price': spot_price,
                'inventory_imbalance': inventory_result['inventory_imbalance'],
                'excess_token': inventory_result['excess_token'],
                'target_rebalance': inventory_result['target_rebalance'],
                'token_a_ratio': inventory_result['token_values']['token_a_ratio'],
                'token_b_ratio': inventory_result['token_values']['token_b_ratio'],
                'total_value_usd': inventory_result['token_values']['total_value_usd'],
                'token_a_range_percent': inventory_result['token_a_range_percent'],
                'token_b_range_percent': inventory_result['token_b_range_percent']
            }
            
        except Exception as e:
            logger.error(f"Error getting inventory status: {e}")
            return {}
    
    def create_single_sided_positions(self, token0: str, token1: str, fee: int, 
                                    spot_price: float, range_a: float, range_b: float) -> Dict[str, Any]:
        """
        Create two single-sided LP positions
        
        Args:
            token0: Token0 address
            token1: Token1 address
            fee: Fee tier
            spot_price: Current spot price
            range_a: Range percentage for position A
            range_b: Range percentage for position B
            
        Returns:
            Transaction results
        """
        try:
            # Get current tick
            pool_address = self.client.get_pool_address(token0, token1, fee)
            pool_info = self.client.get_pool_info(pool_address)
            current_tick = pool_info['tick']
            
            # Calculate tick spacing
            tick_spacing = self.utils.calculate_tick_spacing(fee)
            
            # Position A: Above spot price by 1 tick
            tick_a_lower = ((current_tick + tick_spacing) // tick_spacing) * tick_spacing
            tick_a_upper = self.utils.get_tick_at_price(
                spot_price * (1 + range_a / 100), tick_spacing
            )
            
            # Position B: Below spot price by 1 tick
            tick_b_upper = ((current_tick - tick_spacing) // tick_spacing) * tick_spacing
            tick_b_lower = self.utils.get_tick_at_price(
                spot_price * (1 - range_b / 100), tick_spacing
            )
            
            logger.info(f"Creating Position A: ticks {tick_a_lower} to {tick_a_upper}")
            logger.info(f"Creating Position B: ticks {tick_b_lower} to {tick_b_upper}")
            
            # Get wallet balances
            token0_balance, token1_balance = self.get_wallet_balances(token0, token1)
            
            # Create Position A (above spot)
            result_a = self.lp_manager.add_liquidity(
                token0=token0,
                token1=token1,
                fee=fee,
                amount0_desired=token0_balance // 2,  # Use half of balance
                amount1_desired=0,  # Single-sided position
                tick_lower=tick_a_lower,
                tick_upper=tick_a_upper,
                amount0_min=0,
                amount1_min=0
            )
            
            # Create Position B (below spot)
            result_b = self.lp_manager.add_liquidity(
                token0=token0,
                token1=token1,
                fee=fee,
                amount0_desired=0,  # Single-sided position
                amount1_desired=token1_balance // 2,  # Use half of balance
                tick_lower=tick_b_lower,
                tick_upper=tick_b_upper,
                amount0_min=0,
                amount1_min=0
            )
            
            return {
                'position_a': result_a,
                'position_b': result_b,
                'spot_price': spot_price,
                'range_a': range_a,
                'range_b': range_b
            }
            
        except Exception as e:
            logger.error(f"Error creating single-sided positions: {e}")
            return {'error': str(e)}
    
    def collect_fees_and_burn_position(self, token_id: int) -> Dict[str, Any]:
        """
        Collect fees and burn (remove) a position completely
        
        Args:
            token_id: NFT token ID of the position
            
        Returns:
            Transaction results
        """
        try:
            logger.info(f"Collecting fees and burning position {token_id}")
            
            # Get position info to check for fees owed
            position_info = self.client.get_position_info(token_id)
            fees_owed0 = position_info['tokens_owed0']
            fees_owed1 = position_info['tokens_owed1']
            
            logger.info(f"Fees owed: {fees_owed0} token0, {fees_owed1} token1")
            
            # Collect fees first (if any)
            if fees_owed0 > 0 or fees_owed1 > 0:
                collect_result = self._collect_fees(token_id, fees_owed0, fees_owed1)
                if not collect_result.get('success', False):
                    logger.warning(f"Failed to collect fees for position {token_id}")
            
            # Burn the position (remove all liquidity)
            burn_result = self._burn_position(token_id)
            
            return {
                'success': burn_result.get('success', False),
                'fees_collected': {'token0': fees_owed0, 'token1': fees_owed1},
                'burn_result': burn_result
            }
            
        except Exception as e:
            logger.error(f"Error collecting fees and burning position {token_id}: {e}")
            return {'success': False, 'error': str(e)}
    
    def _collect_fees(self, token_id: int, amount0: int, amount1: int) -> Dict[str, Any]:
        """Collect fees from a position"""
        try:
            logger.info(f"Collecting {amount0} token0 and {amount1} token1 fees from position {token_id}")
            
            # Prepare collect transaction
            collect_params = {
                'tokenId': token_id,
                'recipient': self.client.wallet_address,
                'amount0Max': amount0,
                'amount1Max': amount1
            }
            
            # Build transaction
            transaction = self.client.position_manager.functions.collect(**collect_params).build_transaction({
                'from': self.client.wallet_address,
                'gas': self.client.estimate_gas({
                    'from': self.client.wallet_address,
                    'to': self.config.UNISWAP_V3_POSITION_MANAGER,
                    'data': self.client.position_manager.functions.collect(**collect_params).build_transaction()['data']
                }),
                'gasPrice': self.client.get_gas_price(),
                'nonce': self.client.w3.eth.get_transaction_count(self.client.wallet_address),
                'value': 0
            })
            
            # Sign and send transaction
            signed_txn = self.client.account.sign_transaction(transaction)
            tx_hash = self.client.w3.eth.send_raw_transaction(signed_txn.rawTransaction)
            
            logger.info(f"Fee collection transaction sent: {tx_hash.hex()}")
            
            # Wait for confirmation
            receipt = self.client.w3.eth.wait_for_transaction_receipt(tx_hash)
            
            if receipt.status == 1:
                logger.info("Fees collected successfully!")
                return {'success': True, 'amount0': amount0, 'amount1': amount1, 'tx_hash': tx_hash.hex()}
            else:
                logger.error("Fee collection transaction failed!")
                return {'success': False, 'error': 'Transaction failed', 'tx_hash': tx_hash.hex()}
            
        except Exception as e:
            logger.error(f"Error collecting fees: {e}")
            return {'success': False, 'error': str(e)}
    
    def _burn_position(self, token_id: int) -> Dict[str, Any]:
        """Burn (remove) a position completely"""
        try:
            # Get position info to get liquidity amount
            position_info = self.client.get_position_info(token_id)
            liquidity = position_info['liquidity']
            
            logger.info(f"Burning position {token_id} with liquidity {liquidity}")
            
            # Remove all liquidity
            burn_result = self.lp_manager.remove_liquidity(
                token_id=token_id,
                liquidity_amount=liquidity,
                amount0_min=0,
                amount1_min=0
            )
            
            return burn_result
            
        except Exception as e:
            logger.error(f"Error burning position {token_id}: {e}")
            return {'success': False, 'error': str(e)}
    
    @retry_on_failure(max_retries=3, delay=2.0)
    def rebalance_positions(self, token0: str, token1: str, fee: int) -> Dict[str, Any]:
        """
        Rebalance LP positions with fee collection and position burning
        
        Args:
            token0: Token0 address
            token1: Token1 address
            fee: Fee tier
            
        Returns:
            Rebalancing results
        """
        try:
            logger.info("Starting rebalancing process...")
            
            # Get existing positions from memory
            existing_positions = self.current_positions.copy()
            total_fees_collected = {'token0': 0, 'token1': 0}
            
            logger.info(f"Processing {len(existing_positions)} existing positions")
            
            # Collect fees and burn each position
            for position in existing_positions:
                token_id = position.get('token_id')
                if token_id:
                    logger.info(f"Processing position {token_id}")
                    
                    # Collect fees and burn position
                    burn_result = self.collect_fees_and_burn_position(token_id)
                    
                    if burn_result.get('success', False):
                        fees = burn_result.get('fees_collected', {'token0': 0, 'token1': 0})
                        total_fees_collected['token0'] += fees['token0']
                        total_fees_collected['token1'] += fees['token1']
                        logger.info(f"Successfully burned position {token_id}")
                    else:
                        logger.error(f"Failed to burn position {token_id}: {burn_result.get('error', 'Unknown error')}")
            
            logger.info(f"Total fees collected: {total_fees_collected}")
            
            # Verify all positions have been burned
            if not self.verify_positions_burned():
                logger.error("Not all positions were successfully burned!")
                return {'error': 'Failed to burn all positions'}
            
            # Clear positions from memory
            self.current_positions = []
            
            # Get current spot price
            spot_price = self.get_current_spot_price(token0, token1, fee)
            if spot_price == 0:
                raise ValueError("Could not get spot price")
            
            logger.info(f"Current spot price: {spot_price}")
            
            # Get updated wallet balances (including collected fees)
            token0_balance, token1_balance = self.get_wallet_balances(token0, token1)
            logger.info(f"Updated balances: Token0={token0_balance}, Token1={token1_balance}")
            
            # Calculate dynamic ranges
            range_a, range_b = self.calculate_dynamic_ranges(token0_balance, token1_balance, spot_price)
            logger.info(f"Calculated ranges: A={range_a}%, B={range_b}%")
            
            # Create new single-sided positions
            result = self.create_single_sided_positions(token0, token1, fee, spot_price, range_a, range_b)
            
            # Update positions in memory with new positions
            if result.get('success', False):
                # Extract new position IDs from transaction logs (simplified)
                # In practice, you'd parse the transaction logs to get the actual token IDs
                new_positions = [
                    {'token_id': 'new_position_a', 'status': 'created'},
                    {'token_id': 'new_position_b', 'status': 'created'}
                ]
                self.current_positions = new_positions
            
            # Add fee collection info to result
            result['fees_collected'] = total_fees_collected
            result['positions_burned'] = len(existing_positions)
            
            self.last_rebalance_time = time.time()
            logger.info("Rebalancing completed successfully")
            
            # Send Telegram notification
            try:
                # Get token symbols for notification
                token_a_info = self.client.get_token_info(token0)
                token_b_info = self.client.get_token_info(token1)
                
                # Format fees collected
                fees_formatted = {}
                if total_fees_collected:
                    fees_formatted[f"{token_a_info['symbol']}"] = total_fees_collected.get('amount0', 0)
                    fees_formatted[f"{token_b_info['symbol']}"] = total_fees_collected.get('amount1', 0)
                
                # Format ranges
                ranges_formatted = {
                    'token_a_range': result.get('token_a_range', 0),
                    'token_b_range': result.get('token_b_range', 0)
                }
                
                self.alert_manager.send_rebalance_notification(
                    token_a_symbol=token_a_info['symbol'],
                    token_b_symbol=token_b_info['symbol'],
                    spot_price=spot_price,
                    inventory_ratio=inventory_result.get('inventory_ratio', 0),
                    ranges=ranges_formatted,
                    fees_collected=fees_formatted,
                    gas_used=result.get('gas_used', 0)
                )
            except Exception as alert_error:
                logger.warning(f"Failed to send Telegram notification: {alert_error}")
            
            # Publish rebalance event to CeFi MM agent
            try:
                self.inventory_publisher.publish_rebalance_event(
                    token_a_symbol=token_a_info['symbol'],
                    token_b_symbol=token_b_info['symbol'],
                    spot_price=spot_price,
                    old_ratio=inventory_result.get('old_inventory_ratio', 0),
                    new_ratio=inventory_result.get('inventory_ratio', 0),
                    fees_collected=fees_formatted,
                    gas_used=result.get('gas_used', 0),
                    ranges=ranges_formatted
                )
            except Exception as publish_error:
                logger.warning(f"Failed to publish rebalance event: {publish_error}")
            
            return result
            
        except Exception as e:
            logger.error(f"Error during rebalancing: {e}")
            
            # Send error notification
            try:
                self.alert_manager.send_error_notification(
                    error_type="Rebalance Failed",
                    error_message=str(e),
                    context={
                        'token0': token0,
                        'token1': token1,
                        'fee': fee,
                        'retry_count': 'Max retries exceeded'
                    }
                )
            except Exception as alert_error:
                logger.warning(f"Failed to send error notification: {alert_error}")
            
            # Publish error event to CeFi MM agent
            try:
                self.inventory_publisher.publish_error_event(
                    error_type="Rebalance Failed",
                    error_message=str(e),
                    context={
                        'token0': token0,
                        'token1': token1,
                        'fee': fee,
                        'retry_count': 'Max retries exceeded'
                    }
                )
            except Exception as publish_error:
                logger.warning(f"Failed to publish error event: {publish_error}")
            
            return {'error': str(e)}
    
    def monitoring_loop(self, token0: str, token1: str, fee: int):
        """
        Main monitoring loop - only monitors spot price, positions tracked in memory
        
        Args:
            token0: Token0 address
            token1: Token1 address
            fee: Fee tier
        """
        logger.info("Starting monitoring loop...")
        
        while self.is_running:
            try:
                # Get current spot price
                spot_price = self.get_current_spot_price(token0, token1, fee)
                
                if spot_price != self.last_spot_price:
                    logger.info(f"Spot price changed: {self.last_spot_price} -> {spot_price}")
                    self.last_spot_price = spot_price
                    self.price_history.append({
                        'timestamp': time.time(),
                        'price': spot_price
                    })
                    
                    # Keep only last VOLATILITY_WINDOW_SIZE price points
                    max_history = self.config.VOLATILITY_WINDOW_SIZE
                    if len(self.price_history) > max_history:
                        self.price_history = self.price_history[-max_history:]
                
                # Get current positions from memory
                positions = self.get_position_ranges()
                
                # Get inventory status and publish to CeFi MM agent on every monitoring cycle
                inventory_status = self.get_inventory_status(spot_price)
                if inventory_status:
                    # Publish inventory data to CeFi MM agent
                    try:
                        # Get token symbols for publishing
                        token_a_info = self.client.get_token_info(token0)
                        token_b_info = self.client.get_token_info(token1)
                        
                        # Calculate ranges for publishing
                        ranges = {
                            'token_a_range': inventory_status['token_a_range_percent'],
                            'token_b_range': inventory_status['token_b_range_percent']
                        }
                        
                        # Get current volatility from price history
                        volatility = self.inventory_model.calculate_volatility(self.price_history)
                        
                        self.inventory_publisher.update_inventory_data(
                            token_a_symbol=token_a_info['symbol'],
                            token_b_symbol=token_b_info['symbol'],
                            token_a_address=token0,
                            token_b_address=token1,
                            spot_price=spot_price,
                            inventory_ratio=inventory_status['token_a_ratio'],
                            token_a_balance=inventory_status['token_a_balance'],
                            token_b_balance=inventory_status['token_b_balance'],
                            token_a_usd_value=inventory_status['token_a_usd_value'],
                            token_b_usd_value=inventory_status['token_b_usd_value'],
                            total_usd_value=inventory_status['total_value_usd'],
                            target_ratio=self.config.TARGET_INVENTORY_RATIO,
                            deviation=inventory_status['inventory_imbalance'],
                            should_rebalance=inventory_status['target_rebalance'],
                            volatility=volatility,
                            ranges=ranges
                        )
                    except Exception as publish_error:
                        logger.warning(f"Failed to publish inventory data: {publish_error}")
                
                # Log position summary and inventory status every 5 minutes
                if int(time.time()) % 300 == 0:
                    logger.info(f"Position Summary: {len(positions)} active positions")
                    for pos in positions:
                        logger.info(f"  Position {pos['token_id']}: "
                                  f"Range {pos['tick_lower']}-{pos['tick_upper']}, "
                                  f"Liquidity {pos['liquidity']}, "
                                  f"Fees owed: {pos['tokens_owed0']}/{pos['tokens_owed1']}")
                    
                    # Log inventory status (detailed logging every 5 minutes)
                    if inventory_status:
                        logger.info(f"Inventory Status:")
                        logger.info(f"  Spot Price: {inventory_status['spot_price']:.6f}")
                        logger.info(f"  Token A Ratio: {inventory_status['token_a_ratio']:.3f}")
                        logger.info(f"  Token B Ratio: {inventory_status['token_b_ratio']:.3f}")
                        logger.info(f"  Inventory Imbalance: {inventory_status['inventory_imbalance']:.3f}")
                        logger.info(f"  Excess Token: {inventory_status['excess_token']}")
                        logger.info(f"  Target Rebalance: {inventory_status['target_rebalance']}")
                        logger.info(f"  Total Value USD: ${inventory_status['total_value_usd']:.2f}")
                        logger.info(f"  Range A: {inventory_status['token_a_range_percent']:.2f}%")
                        logger.info(f"  Range B: {inventory_status['token_b_range_percent']:.2f}%")
                
                # Check if rebalancing is needed
                if self.should_rebalance(spot_price, positions):
                    logger.info("Rebalancing triggered")
                    result = self.rebalance_positions(token0, token1, fee)
                    
                    if 'error' in result:
                        logger.error(f"Rebalancing failed: {result['error']}")
                    else:
                        logger.info("Rebalancing successful")
                        logger.info(f"Fees collected: {result.get('fees_collected', {})}")
                        logger.info(f"Positions burned: {result.get('positions_burned', 0)}")
                
                # Wait for next check
                time.sleep(self.config.MONITORING_INTERVAL_SECONDS)
                
            except Exception as e:
                logger.error(f"Error in monitoring loop: {e}")
                time.sleep(10)  # Wait 10 seconds before retrying
    
    def start_monitoring(self, token0: str, token1: str, fee: int):
        """
        Start the monitoring process
        
        Args:
            token0: Token0 address
            token1: Token1 address
            fee: Fee tier
        """
        if self.is_running:
            logger.warning("Monitoring is already running")
            return
        
        # Validate token ordering before starting
        logger.info("Validating token ordering configuration...")
        if not self.validate_token_ordering(token0, token1, fee):
            logger.error("Token ordering validation failed. Exiting to prevent incorrect calculations.")
            logger.error("Please fix your TOKEN_A_ADDRESS and TOKEN_B_ADDRESS configuration.")
            return
        
        # Initialize positions on startup
        if not self.initialize_positions():
            logger.error("Failed to initialize positions on startup")
            return
        
        self.is_running = True
        
        self.monitoring_thread = threading.Thread(
            target=self.monitoring_loop,
            args=(token0, token1, fee),
            daemon=True
        )
        self.monitoring_thread.start()
        
        logger.info("Monitoring started")
    
    def stop_monitoring(self):
        """Stop the monitoring process"""
        if not self.is_running:
            logger.warning("Monitoring is not running")
            return
        
        self.is_running = False
        
        if self.monitoring_thread:
            self.monitoring_thread.join(timeout=5)
        
        logger.info("Monitoring stopped")
    
    def get_status(self) -> Dict[str, Any]:
        """Get current status of the rebalancer"""
        current_positions = self.get_position_ranges()
        return {
            'is_running': self.is_running,
            'last_spot_price': self.last_spot_price,
            'last_rebalance_time': self.last_rebalance_time,
            'price_history_count': len(self.price_history),
            'current_positions': len(current_positions),
            'position_details': current_positions,
            'monitoring_interval': self.config.MONITORING_INTERVAL_SECONDS,
            'rebalance_threshold': self.config.REBALANCE_THRESHOLD_PERCENTAGE
        }