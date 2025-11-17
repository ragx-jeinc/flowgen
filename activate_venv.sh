#!/bin/bash
# FlowGen Virtual Environment Activation Script
#
# Usage: source activate_venv.sh

# Activate virtual environment
source venv/bin/activate

# Set library paths
export PYTHONPATH=/pxstore/claude/projects/flowgen/flowgen:$PYTHONPATH
export LD_LIBRARY_PATH=/pxstore/claude/projects/flowgen/flowgen/flowgen:$LD_LIBRARY_PATH

echo "✓ FlowGen virtual environment activated"
echo "  Python: $(which python3)"
echo "  PYTHONPATH: $PYTHONPATH"
echo "  LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
echo ""
echo "To test: python -c 'from flowgen import HAS_CPP_CORE; print(f\"C++ Core: {HAS_CPP_CORE}\")'"
echo "To run example: python examples/basic_usage.py"
