"""
Model factory for creating inventory management models.
"""
import logging
from typing import Type, Dict, Any
from config import Config
from .base_model import BaseInventoryModel
from .avellaneda_stoikov import AvellanedaStoikovModel
from .simple_model import SimpleModel

logger = logging.getLogger(__name__)


class ModelFactory:
    """
    Factory class for creating inventory management models.
    
    This allows easy switching between different models without
    changing the core rebalancing logic.
    """
    
    # Registry of available models
    _models: Dict[str, Type[BaseInventoryModel]] = {
        'AvellanedaStoikovModel': AvellanedaStoikovModel,
        'SimpleModel': SimpleModel,
        # Add more models here as they are implemented
        # 'MLModel': MLModel,
        # 'HybridModel': HybridModel,
    }
    
    @classmethod
    def create_model(cls, model_name: str, config: Config) -> BaseInventoryModel:
        """
        Create an inventory management model instance.
        
        Args:
            model_name: Name of the model to create
            config: Configuration object
            
        Returns:
            Model instance
            
        Raises:
            ValueError: If model name is not found
        """
        if model_name not in cls._models:
            available_models = ', '.join(cls._models.keys())
            raise ValueError(f"Unknown model '{model_name}'. Available models: {available_models}")
        
        model_class = cls._models[model_name]
        model_instance = model_class(config)
        
        logger.info(f"Created {model_name} instance")
        return model_instance
    
    @classmethod
    def get_available_models(cls) -> Dict[str, str]:
        """
        Get list of available models with descriptions.
        
        Returns:
            Dictionary mapping model names to descriptions
        """
        models_info = {}
        for model_name, model_class in cls._models.items():
            # Create a temporary instance to get model info
            try:
                temp_config = Config()
                temp_instance = model_class(temp_config)
                model_info = temp_instance.get_model_info()
                models_info[model_name] = model_info.get('description', 'No description available')
            except Exception as e:
                logger.warning(f"Could not get info for model {model_name}: {e}")
                models_info[model_name] = 'Description not available'
        
        return models_info
    
    @classmethod
    def register_model(cls, name: str, model_class: Type[BaseInventoryModel]):
        """
        Register a new model class.
        
        Args:
            name: Name of the model
            model_class: Model class that inherits from BaseInventoryModel
        """
        if not issubclass(model_class, BaseInventoryModel):
            raise ValueError(f"Model class must inherit from BaseInventoryModel")
        
        cls._models[name] = model_class
        logger.info(f"Registered new model: {name}")
    
    @classmethod
    def get_model_info(cls, model_name: str) -> Dict[str, Any]:
        """
        Get detailed information about a specific model.
        
        Args:
            model_name: Name of the model
            
        Returns:
            Dictionary with model information
            
        Raises:
            ValueError: If model name is not found
        """
        if model_name not in cls._models:
            available_models = ', '.join(cls._models.keys())
            raise ValueError(f"Unknown model '{model_name}'. Available models: {available_models}")
        
        try:
            temp_config = Config()
            model_class = cls._models[model_name]
            temp_instance = model_class(temp_config)
            return temp_instance.get_model_info()
        except Exception as e:
            logger.error(f"Error getting model info for {model_name}: {e}")
            return {
                'name': model_name,
                'description': 'Error loading model information',
                'version': 'unknown',
                'parameters': {},
                'features': []
            }
