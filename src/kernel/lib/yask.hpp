/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2017, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

// This file defines functions, types, and macros needed for the stencil
// kernel.

#ifndef YASK_HPP
#define YASK_HPP

// Include the API first. This helps to ensure that it will stand alone.
#include "yask_kernel_api.hpp"

// Settings from makefile.
#include "yask_macros.hpp"

// Control assert() by turning on with DEBUG instead of turning off with
// NDEBUG. This makes it off by default.
#ifndef DEBUG
#define NDEBUG
#endif

#include <cstdint>
#include <iostream>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <initializer_list>

#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#include <stdint.h>
#include <immintrin.h>
#endif

// Floored integer divide and mod.
#include "idiv.hpp"

// Macros for 1D<->nD transforms.
#include "yask_layout_macros.hpp"

// Auto-generated macros from foldBuilder.
// It's important that this be included before realv.hpp
// to properly set the vector lengths.
#define DEFINE_MACROS
#include "yask_stencil_code.hpp"
#undef DEFINE_MACROS

// Define a folded vector of reals.
#include "realv.hpp"

// Other utilities.
#include "utils.hpp"

// macro for debug message.
#ifdef TRACE
#define TRACE_MSG0(os, msg) ((os) << "YASK: " << msg << std::endl << std::flush)
#else
#define TRACE_MSG0(os, msg) ((void)0)
#endif

// macro for debug message from a StencilContext method.
#define TRACE_MSG1(msg) TRACE_MSG0(get_ostr(), msg)
#define TRACE_MSG(msg) TRACE_MSG1(msg)
 
// macro for debug message when _context ptr is defined.
#define TRACE_MSG2(msg) TRACE_MSG0(_context->get_ostr(), msg)
 
// macro for debug message when _generic_context ptr is defined.
#define TRACE_MSG3(msg) TRACE_MSG0(_generic_context->get_ostr(), msg)

// L1 and L2 hints
#define L1 _MM_HINT_T0
#define L2 _MM_HINT_T1

////// Default prefetch distances.
// These are only used if and when prefetch code is generated by
// gen-loops.pl.

// How far to prefetch ahead for L1.
#ifndef PFDL1
 #define PFDL1 1
#endif

// How far to prefetch ahead for L2.
#ifndef PFDL2
 #define PFDL2 2
#endif

// Set MODEL_CACHE to 1 or 2 to model L1 or L2.
#ifdef MODEL_CACHE
#include "cache_model.hpp"
extern yask::Cache cache_model;
 #if MODEL_CACHE==L1
  #warning Modeling L1 cache
 #elif MODEL_CACHE==L2
  #warning Modeling L2 cache
 #else
  #warning Modeling UNKNOWN cache
 #endif
#endif

#include "tuple.hpp"
namespace yask {

    typedef Tuple<idx_t> IdxTuple;
    typedef std::vector<idx_t> GridIndices;
    typedef std::vector<std::string> GridDimNames;

    // Dimensions for a solution.
    // Similar to that in the YASK compiler.
    struct Dims {

        static const int max_domain_dims = MAX_DIMS - 1; // 1 reserved for step dim.

        // Dimensions with unused values.
        std::string _step_dim;
        IdxTuple _domain_dims;
        IdxTuple _stencil_dims;
        IdxTuple _misc_dims;

        // Dimensions and sizes.
        IdxTuple _fold_pts;
        IdxTuple _cluster_pts;
        IdxTuple _cluster_mults;

        // Check whether dim exists and is of allowed type.
        // If not, abort with error, reporting 'fn_name'.
        void checkDimType(const std::string& dim,
                          const std::string& fn_name,
                          bool step_ok,
                          bool domain_ok,
                          bool misc_ok) const;

    };
    typedef std::shared_ptr<Dims> DimsPtr;
    
    // A class to hold up to a given number of sizes or indices efficiently.
    // Similar to a Tuple, but less overhead and doesn't keep names.
    // Make sure this stays non-virtual.
    class Indices {
        
    public:
        static const int max_idxs = MAX_DIMS;
        
    protected:
        idx_t _idxs[max_idxs] = {0};
        
    public:
        // Ctors.
        Indices() { }
        Indices(const IdxTuple& src) {
            setFromTuple(src);
        }
        Indices(const GridIndices& src) {
            setFromVec(src);
        }
        Indices(const std::initializer_list<idx_t>& src) {
            setFromInitList(src);
        }
        Indices(int nelems, const idx_t src[]) {
            setFromArray(nelems, src);
        }
        Indices(idx_t val) {
            setFromConst(val);
        }
        
        // Default copy ctor, copy operator should be okay.

        // Access indices.
        inline idx_t& operator[](int i) {
            assert(i >= 0);
            assert(i < max_idxs);
            return _idxs[i];
        }
        inline idx_t operator[](int i) const {
            assert(i >= 0);
            assert(i < max_idxs);
            return _idxs[i];
        }

        // Sync with IdxTuple.
        void setFromTuple(const IdxTuple& src) {
            assert(src.size() < max_idxs);
            for (int i = 0; i < max_idxs; i++)
                _idxs[i] = (i < src.size()) ? src.getVal(i) : 0;
        }
        void setTupleVals(IdxTuple& tgt) const {
            assert(tgt.size() < max_idxs);
            for (int i = 0; i < max_idxs; i++)
                if (i < tgt.size())
                    tgt.setVal(i, _idxs[i]);
        }
        
        // Other inits.
        void setFromVec(const GridIndices& src) {
            assert(src.size() < max_idxs);
            for (int i = 0; i < max_idxs; i++)
                _idxs[i] = (i < int(src.size())) ? src[i] : 0;
        }
        void setFromInitList(const std::initializer_list<idx_t>& src) {
            assert(src.size() < max_idxs);
            int i = 0;
            for (auto idx : src)
                _idxs[i++] = idx;
        }
        void setFromArray(int nelems, const idx_t src[]) {
            assert(nelems < max_idxs);
            for (int i = 0; i < max_idxs; i++)
                _idxs[i] = (i < nelems) ? src[i] : 0;
        }
        void setFromConst(idx_t val) {
            for (int i = 0; i < max_idxs; i++)
                _idxs[i] = val;
        }
        
        // Some comparisons.
        // These assume all the indices are valid or
        // initialized to the same value.
        bool operator==(const Indices& rhs) const {
            for (int i = 0; i < max_idxs; i++)
                if (_idxs[i] != rhs._idxs[i])
                    return false;
            return true;
        }
        bool operator!=(const Indices& rhs) const {
            return !operator==(rhs);
        }
        bool operator<(const Indices& rhs) const {
            for (int i = 0; i < max_idxs; i++)
                if (_idxs[i] < rhs._idxs[i])
                    return true;
                else if (_idxs[i] > rhs._idxs[i])
                    return false;
            return false;       // equal, so not less than.
        }
        bool operator>(const Indices& rhs) const {
            for (int i = 0; i < max_idxs; i++)
                if (_idxs[i] > rhs._idxs[i])
                    return true;
                else if (_idxs[i] < rhs._idxs[i])
                    return false;
            return false;       // equal, so not greater than.
        }

        // Some element-wise operators.
        Indices minElements(const Indices& other) {
            Indices res;
            for (int i = 0; i < max_idxs; i++)
                res._idxs[i] = std::min(_idxs[i], other._idxs[i]);
            return res;
        }
        Indices maxElements(const Indices& other) {
            Indices res;
            for (int i = 0; i < max_idxs; i++)
                res._idxs[i] = std::max(_idxs[i], other._idxs[i]);
            return res;
        }

        // Operate on all elements.
        Indices addElements(idx_t n) const {
            Indices res;
            for (int i = 0; i < max_idxs; i++)
                res._idxs[i] = _idxs[i] + n;
            return res;
        }
        
        // Make string like "x=4, y=8".
        std::string makeDimValStr(const GridDimNames& names,
                                          std::string separator=", ",
                                          std::string infix="=",
                                          std::string prefix="",
                                          std::string suffix="") const {
            assert(names.size() <= max_idxs);

            // Make a Tuple from names.
            IdxTuple tmp;
            for (int i = 0; i < int(names.size()); i++)
                tmp.addDimBack(names[i], _idxs[i]);
            return tmp.makeDimValStr(separator, infix, prefix, suffix);
        }
        
        // Make string like "4, 3, 2".
        std::string makeValStr(int nvals,
                                       std::string separator=", ",
                                       std::string prefix="",
                                       std::string suffix="") const {
            assert(nvals <= max_idxs);

            // Make a Tuple w/o useful names.
            IdxTuple tmp;
            for (int i = 0; i < nvals; i++)
                tmp.addDimBack(std::string(1, '0' + char(i)), _idxs[i]);
            return tmp.makeValStr(separator, prefix, suffix);
        }
    };

    // Define OMP reductions on Indices.
#pragma omp declare reduction(min_idxs : Indices : \
                              omp_out = omp_out.minElements(omp_in) )   \
    initializer (omp_priv = idx_max)
#pragma omp declare reduction(max_idxs : Indices : \
                              omp_out = omp_out.maxElements(omp_in) )   \
    initializer (omp_priv = idx_min)
        
    // A group of Indices needed for generated loops.
    // See the help message from gen_loops.pl for the
    // documentation of the indices.
    // Make sure this stays non-virtual.
    struct ScanIndices {
        Indices begin, end, step, group_size;
        Indices start, stop, index;

        // Default init.
        ScanIndices() {
            begin = 0;
            end = 0;
            step = 1;
            group_size = 1;
            start = 0;
            stop = 0;
            index = 0;
        }
        
        // Init from outer-loop indices.
        void initFromOuter(const ScanIndices& outer) {

            // Begin & end set from start & stop of outer loop.
            begin = outer.start;
            end = outer.stop;

            // Pass other values through by default.
            start = outer.start;
            stop = outer.stop;
            index = outer.index;
        }
    };
} // namespace yask.

#include "yask_layouts.hpp"
#include "generic_grids.hpp"
#include "realv_grids.hpp"

// First/last index macros.
// These are relative to global problem, not rank.
#define FIRST_INDEX(dim) (0)
#warning FIXME
#define LAST_INDEX(dim) (_context->tot_ ## dim - 1)

// Base types for stencil context, etc.
#include "stencil_calc.hpp"

// Auto-generated stencil code that extends base types.
#define DEFINE_CONTEXT
#include "yask_stencil_code.hpp"
#undef DEFINE_CONTEXT

#endif
