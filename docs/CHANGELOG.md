# Changelog

## [Unreleased]

### Added
- **Architecture Diagram**: Complete visual architecture diagram using Mermaid syntax (`docs/architecture_diagram.md`)
- **Centralized Logging System**: All `std::cout` and `std::cerr` statements replaced with logging macros
- **Log Level Optimization**: Normal trading flow uses DEBUG level, lifecycle events use INFO level

### Changed
- **Logging Migration**: All production code now uses centralized logging macros:
  - `LOG_DEBUG_COMP` for normal trading flow (orders, positions, market data updates)
  - `LOG_INFO_COMP` for lifecycle events (startup, shutdown, configuration)
  - `LOG_WARN_COMP` for warnings and degraded states
  - `LOG_ERROR_COMP` for errors and failures
- **Documentation Updates**: All README and documentation files updated to reflect current status

### Files Modified
- All exchange handler files (Binance, Deribit, GRVT OMS/PMS/Subscribers)
- All trader components (zmq adapters, mini_pms, trader_main)
- All server components (market_server, position_server, trading_engine)
- All utility files (zmq, websocket, handlers, config, app_service)
- All documentation files (README.md, deploy.md, strategy_guide.md, etc.)

### Technical Details
- **Logging Macros**: `LOG_INFO_COMP`, `LOG_DEBUG_COMP`, `LOG_ERROR_COMP`, `LOG_WARN_COMP`
- **Logging System**: Centralized logging via `cpp/utils/logging/logger.hpp` and `log_helper.hpp`
- **Exclusions**: Test files and simulation tools intentionally keep `std::cout` for direct output
- **Logging System Files**: `logger.cpp` and `logger.hpp` intentionally use `cout`/`cerr` for initialization

### Documentation
- Architecture diagram created with Mermaid syntax (renders on GitHub)
- All README files updated with current status
- Strategy guide updated with logging examples
- Deployment guide updated with logging information

