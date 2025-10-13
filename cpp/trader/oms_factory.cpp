#include "i_oms.hpp"
#include "multi_exchange_oms.hpp"

std::unique_ptr<IOMS> OMSFactory::create() {
  return std::make_unique<MultiExchangeOMS>();
}
