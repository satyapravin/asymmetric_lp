#pragma once

class GlftTarget {
public:
  // For now, identity mapping: target inventory equals desired_offset
  double compute_target(double desired_offset) const {
    return desired_offset; // placeholder for full GLFT logic
  }
};


