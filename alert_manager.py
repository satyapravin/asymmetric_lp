"""
AsymmetricLP - Telegram Alert Manager
Handles notifications for rebalances, errors, and system events
"""
import logging
import requests
import json
from typing import Dict, Any, Optional
from datetime import datetime
from config import Config

logger = logging.getLogger(__name__)

class TelegramAlertManager:
    """Manages Telegram notifications for the rebalancer"""
    
    def __init__(self, config: Config):
        """
        Initialize Telegram alert manager
        
        Args:
            config: Configuration object
        """
        self.config = config
        self.bot_token = config.TELEGRAM_BOT_TOKEN
        self.chat_id = config.TELEGRAM_CHAT_ID
        self.enabled = config.TELEGRAM_ENABLED
        
        if self.enabled:
            logger.info("Telegram alerts enabled")
            # Test connection
            if not self._test_connection():
                logger.warning("Telegram connection test failed - alerts may not work")
        else:
            logger.info("Telegram alerts disabled (missing bot token or chat ID)")
    
    def _test_connection(self) -> bool:
        """
        Test Telegram bot connection
        
        Returns:
            True if connection successful, False otherwise
        """
        try:
            url = f"https://api.telegram.org/bot{self.bot_token}/getMe"
            response = requests.get(url, timeout=10)
            return response.status_code == 200
        except Exception as e:
            logger.error(f"Telegram connection test failed: {e}")
            return False
    
    def _send_message(self, message: str, parse_mode: str = "HTML") -> bool:
        """
        Send message to Telegram
        
        Args:
            message: Message to send
            parse_mode: Message parse mode (HTML or Markdown)
            
        Returns:
            True if message sent successfully, False otherwise
        """
        if not self.enabled:
            logger.debug("Telegram alerts disabled - not sending message")
            return False
        
        try:
            url = f"https://api.telegram.org/bot{self.bot_token}/sendMessage"
            data = {
                'chat_id': self.chat_id,
                'text': message,
                'parse_mode': parse_mode,
                'disable_web_page_preview': True
            }
            
            response = requests.post(url, data=data, timeout=10)
            
            if response.status_code == 200:
                logger.debug("Telegram message sent successfully")
                return True
            else:
                logger.error(f"Failed to send Telegram message: {response.status_code} - {response.text}")
                return False
                
        except Exception as e:
            logger.error(f"Error sending Telegram message: {e}")
            return False
    
    def send_rebalance_notification(self, 
                                  token_a_symbol: str, 
                                  token_b_symbol: str,
                                  spot_price: float,
                                  inventory_ratio: float,
                                  ranges: Dict[str, float],
                                  fees_collected: Dict[str, float],
                                  gas_used: int) -> bool:
        """
        Send rebalance notification
        
        Args:
            token_a_symbol: Token A symbol
            token_b_symbol: Token B symbol
            spot_price: Current spot price
            inventory_ratio: Current inventory ratio
            ranges: Calculated ranges for both tokens
            fees_collected: Fees collected from burned positions
            gas_used: Gas used for the rebalance
            
        Returns:
            True if notification sent successfully
        """
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # Format fees collected
        fees_text = ""
        if fees_collected:
            fees_text = f"\n💰 <b>Fees Collected:</b>\n"
            for token, amount in fees_collected.items():
                fees_text += f"  • {token}: {amount:.6f}\n"
        
        # Format ranges
        ranges_text = f"\n📊 <b>New Ranges:</b>\n"
        ranges_text += f"  • {token_a_symbol}: {ranges.get('token_a_range', 0):.2f}%\n"
        ranges_text += f"  • {token_b_symbol}: {ranges.get('token_b_range', 0):.2f}%\n"
        
        message = f"""
🔄 <b>LP Rebalance Completed</b>
⏰ {timestamp}

📈 <b>Market Data:</b>
  • Pair: {token_a_symbol}/{token_b_symbol}
  • Spot Price: {spot_price:.6f}
  • Inventory Ratio: {inventory_ratio:.3f}

{ranges_text}{fees_text}
⛽ <b>Gas Used:</b> {gas_used:,} gas

✅ Rebalance successful!
        """.strip()
        
        return self._send_message(message)
    
    def send_error_notification(self, 
                               error_type: str,
                               error_message: str,
                               context: Optional[Dict[str, Any]] = None) -> bool:
        """
        Send error notification
        
        Args:
            error_type: Type of error (e.g., "Transaction Failed", "Network Error")
            error_message: Error message
            context: Additional context information
            
        Returns:
            True if notification sent successfully
        """
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # Format context if provided
        context_text = ""
        if context:
            context_text = f"\n📋 <b>Context:</b>\n"
            for key, value in context.items():
                context_text += f"  • {key}: {value}\n"
        
        message = f"""
🚨 <b>Error Alert</b>
⏰ {timestamp}

❌ <b>Error Type:</b> {error_type}
💬 <b>Message:</b> {error_message}{context_text}

⚠️ Please check the logs for more details.
        """.strip()
        
        return self._send_message(message)
    
    def send_startup_notification(self, 
                                 token_a_symbol: str,
                                 token_b_symbol: str,
                                 chain_name: str,
                                 wallet_address: str) -> bool:
        """
        Send startup notification
        
        Args:
            token_a_symbol: Token A symbol
            token_b_symbol: Token B symbol
            chain_name: Chain name
            wallet_address: Wallet address
            
        Returns:
            True if notification sent successfully
        """
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        message = f"""
🚀 <b>LP Rebalancer Started</b>
⏰ {timestamp}

🌐 <b>Configuration:</b>
  • Chain: {chain_name}
  • Pair: {token_a_symbol}/{token_b_symbol}
  • Wallet: <code>{wallet_address[:10]}...{wallet_address[-8:]}</code>

✅ System initialized and monitoring started
        """.strip()
        
        return self._send_message(message)
    
    def send_shutdown_notification(self, reason: str = "Manual shutdown") -> bool:
        """
        Send shutdown notification
        
        Args:
            reason: Reason for shutdown
            
        Returns:
            True if notification sent successfully
        """
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        message = f"""
🛑 <b>LP Rebalancer Stopped</b>
⏰ {timestamp}

📝 <b>Reason:</b> {reason}

👋 Monitoring stopped
        """.strip()
        
        return self._send_message(message)
    
    def send_critical_alert(self, 
                           alert_type: str,
                           message: str,
                           action_required: bool = True) -> bool:
        """
        Send critical alert requiring immediate attention
        
        Args:
            alert_type: Type of critical alert
            message: Alert message
            action_required: Whether immediate action is required
            
        Returns:
            True if notification sent successfully
        """
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        urgency = "🚨 IMMEDIATE ACTION REQUIRED" if action_required else "⚠️ Attention Required"
        
        alert_message = f"""
{urgency}
⏰ {timestamp}

🔴 <b>Critical Alert:</b> {alert_type}
💬 <b>Message:</b> {message}

{'⚡ Please take immediate action!' if action_required else '📋 Please review when convenient.'}
        """.strip()
        
        return self._send_message(alert_message)
