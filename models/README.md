# Inventory Management Models

This directory contains the inventory management models for AsymmetricLP. The models are designed to be easily swappable without changing the core rebalancing logic.

## Architecture

### Base Model (`base_model.py`)
All models inherit from `BaseInventoryModel` which defines the interface:
- `calculate_lp_ranges()` - Main method for calculating ranges
- `get_model_info()` - Returns model information
- `validate_inputs()` - Input validation
- `calculate_inventory_ratio()` - Helper method for inventory calculations
- `calculate_volatility()` - Basic volatility calculation

### Model Factory (`model_factory.py`)
The factory pattern allows easy model switching:
- `create_model()` - Creates model instances
- `get_available_models()` - Lists available models
- `register_model()` - Register new models dynamically

## Available Models

### 1. AvellanedaStoikovModel (`avellaneda_stoikov.py`)
The default model implementing Avellaneda-Stoikov market making theory:
- Dynamic range calculation based on inventory imbalance
- Volatility-adjusted spreads
- Parkinson volatility estimator
- Risk aversion parameter

### 2. SimpleModel (`simple_model.py`)
A basic example model for demonstration:
- Fixed base range with inventory scaling
- Simple inventory-based adjustment
- No volatility consideration
- Easy to understand and modify

## Adding a New Model

### Step 1: Create the Model Class
```python
# models/my_model.py
from .base_model import BaseInventoryModel

class MyModel(BaseInventoryModel):
    def __init__(self, config):
        super().__init__(config)
        # Initialize your model parameters
        
    def calculate_lp_ranges(self, token0_balance, token1_balance, spot_price, 
                           price_history, token_a_address, token_b_address, client):
        # Your range calculation logic here
        return {
            'range_a_percentage': range_a * 100.0,
            'range_b_percentage': range_b * 100.0,
            'inventory_ratio': current_ratio,
            'target_ratio': target_ratio,
            'deviation': deviation,
            'volatility': volatility,
            'model_metadata': {
                'model_name': self.model_name,
                'your_custom_params': values
            }
        }
    
    def get_model_info(self):
        return {
            'name': 'My Model',
            'description': 'Description of your model',
            'version': '1.0.0',
            'parameters': {...},
            'features': [...]
        }
```

### Step 2: Register the Model
```python
# models/model_factory.py
from .my_model import MyModel

class ModelFactory:
    _models = {
        'AvellanedaStoikovModel': AvellanedaStoikovModel,
        'SimpleModel': SimpleModel,
        'MyModel': MyModel,  # Add your model here
    }
```

### Step 3: Update Imports
```python
# models/__init__.py
from .my_model import MyModel
__all__ = [..., 'MyModel']
```

### Step 4: Use the Model
Set in your `.env` file:
```bash
INVENTORY_MODEL=MyModel
```

## Model Interface

All models must implement:

### `calculate_lp_ranges()`
**Input:**
- `token0_balance`: Token0 balance in wei
- `token1_balance`: Token1 balance in wei  
- `spot_price`: Current spot price
- `price_history`: Historical price data
- `token_a_address`: Token A address
- `token_b_address`: Token B address
- `client`: Uniswap client for additional data

**Output:**
- `range_a_percentage`: Range percentage for position A
- `range_b_percentage`: Range percentage for position B
- `inventory_ratio`: Current inventory ratio
- `target_ratio`: Target inventory ratio
- `deviation`: Current deviation from target
- `volatility`: Calculated volatility
- `model_metadata`: Model-specific data

### `get_model_info()`
**Output:**
- `name`: Model name
- `description`: Model description
- `version`: Model version
- `parameters`: Model parameters
- `features`: List of features

## Helper Methods

The base class provides useful helper methods:

### `calculate_inventory_ratio()`
Calculates current inventory ratio (token0 value / total value)

### `calculate_volatility()`
Basic volatility calculation using log returns

### `validate_inputs()`
Validates input parameters

## Configuration

Models can access configuration through `self.config`:
```python
# In your model
my_param = getattr(self.config, 'MY_PARAM', default_value)
```

Add new parameters to `config.py` and `env.example` as needed.

## Testing Models

You can test models independently:
```python
from models import ModelFactory
from config import Config

config = Config()
model = ModelFactory.create_model('MyModel', config)
info = model.get_model_info()
print(info)
```

## Best Practices

1. **Inherit from BaseInventoryModel** - Don't implement the interface from scratch
2. **Use helper methods** - Leverage base class functionality
3. **Handle errors gracefully** - Return fallback values on errors
4. **Log important events** - Use the logger for debugging
5. **Validate inputs** - Use the base class validation
6. **Document parameters** - Include all parameters in `get_model_info()`
7. **Test thoroughly** - Test with different market conditions
8. **Keep it simple** - Start with simple models and add complexity gradually

## Example: Creating a Mean Reversion Model

```python
class MeanReversionModel(BaseInventoryModel):
    def __init__(self, config):
        super().__init__(config)
        self.mean_reversion_strength = 0.5
        self.lookback_period = 20
        
    def calculate_lp_ranges(self, token0_balance, token1_balance, spot_price, 
                           price_history, token_a_address, token_b_address, client):
        # Calculate mean price over lookback period
        if len(price_history) >= self.lookback_period:
            recent_prices = [p['price'] for p in price_history[-self.lookback_period:]]
            mean_price = sum(recent_prices) / len(recent_prices)
            
            # Calculate deviation from mean
            price_deviation = abs(spot_price - mean_price) / mean_price
            
            # Adjust ranges based on mean reversion
            base_range = 0.05
            if price_deviation > 0.1:  # Price far from mean
                range_multiplier = 1 + self.mean_reversion_strength * price_deviation
            else:  # Price close to mean
                range_multiplier = 1 - self.mean_reversion_strength * price_deviation
                
            range_a = base_range * range_multiplier
            range_b = base_range * range_multiplier
            
            return {
                'range_a_percentage': range_a * 100.0,
                'range_b_percentage': range_b * 100.0,
                'inventory_ratio': 0.5,  # Assume balanced for simplicity
                'target_ratio': 0.5,
                'deviation': 0.0,
                'volatility': self.calculate_volatility(price_history),
                'model_metadata': {
                    'model_name': self.model_name,
                    'mean_reversion_strength': self.mean_reversion_strength,
                    'price_deviation': price_deviation
                }
            }
```

This architecture makes it easy to experiment with different models and find what works best for your specific use case.
