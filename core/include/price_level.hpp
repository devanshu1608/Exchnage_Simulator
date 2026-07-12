#pragma once
#include "order.hpp"
#include <list>
#include <algorithm>

namespace exchange {

class PriceLevel {
public:
  PriceLevel(Price price) : mPrice(price) {}

  Price getPrice() const { return mPrice; }
  Quantity getTotalQty() const { return mTotalQty; }
  bool isEmpty() const { return mOrders.empty(); }

  /// Adds an order to the back of the queue (time priority)
  void addOrder(BookElement *order) {
      mOrders.push_back(order);
      mTotalQty += order->m_quantity;
  }

  /// Removes an order from the level
  void removeOrder(BookElement *order) {
      auto it = std::find(mOrders.begin(), mOrders.end(), order);
      if (it != mOrders.end()) {
          mTotalQty -= (*it)->m_quantity;
          mOrders.erase(it);
      }
  }

  /// Reduces the quantity of an order in the level
  void reduceQty(Quantity amt) {
      mTotalQty -= amt;
  }

  /// Gets the first order in line
  BookElement *peekFirst() const {
      if (mOrders.empty()) return nullptr;
      return mOrders.front();
  }

  /// Get all orders (for matching)
  std::list<BookElement *> &getOrders() { return mOrders; }

private:
  Price mPrice;
  Quantity mTotalQty{0};

  // V1 uses std::list, V2 will use an intrusive list
  std::list<BookElement *> mOrders;
};

} // namespace exchange
