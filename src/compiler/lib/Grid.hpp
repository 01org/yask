/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2019, Intel Corporation

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

///////// Classes for Grids ////////////

#pragma once

#include "Expr.hpp"

using namespace std;

namespace yask {

    // Fwd decl.
    struct Dimensions;

    // A class for a Grid.
    // This is a generic container for all variables to be accessed
    // from the kernel. A 0-D grid is a scalar, a 1-D grid is an array, etc.
    // Dims can be the step dim, a domain dim, or anything else.
    class Grid : public virtual yc_grid {

    protected:
        string _name;           // name of this grid.
        IndexExprPtrVec _dims;  // dimensions of this grid.
        bool _isScratch = false; // true if a temp grid.

        // Step-dim info.
        bool _isStepAllocFixed = true; // step alloc cannot be changed at run-time.
        idx_t _stepAlloc = 0;         // step-alloc override (0 => calculate).

        // Ptr to solution that this grid belongs to (its parent).
        StencilSolution* _soln = 0;

        // How many dims are foldable.
        int _numFoldableDims = -1; // -1 => unknown.

        // Whether this grid can be vector-folded.
        bool _isFoldable = false;

        ///// Values below are computed based on GridPoint accesses in equations.

        // Min and max const indices that are used to access each dim.
        IntTuple _minIndices, _maxIndices;

        // Max abs-value of domain-index halos required by all eqs at
        // various step-index values.
        // string key: name of pack.
        // bool key: true=left, false=right.
        // int key: step-dim offset or 0 if no step-dim.
        map<string, map<bool, map<int, IntTuple>>> _halos;

    public:
        // Ctors.
        Grid(string name,
             bool isScratch,
             StencilSolution* soln,
             const IndexExprPtrVec& dims);
        Grid(string name,
             bool isScratch,
             StencilSolution* soln,
             IndexExprPtr dim1 = nullptr,
             IndexExprPtr dim2 = nullptr,
             IndexExprPtr dim3 = nullptr,
             IndexExprPtr dim4 = nullptr,
             IndexExprPtr dim5 = nullptr,
             IndexExprPtr dim6 = nullptr);

        // Dtor.
        virtual ~Grid() { }

        // Name accessors.
        const string& getName() const { return _name; }
        void setName(const string& name) { _name = name; }
        string getDescr() const;

        // Access dims.
        virtual const IndexExprPtrVec& getDims() const { return _dims; }

        // Step dim or null if none.
        virtual const IndexExprPtr getStepDim() const {
            for (auto d : _dims)
                if (d->getType() == STEP_INDEX)
                    return d;
            return nullptr;
        }

        // Temp grid?
        virtual bool isScratch() const { return _isScratch; }

        // Access to solution.
        virtual StencilSolution* getSoln() { return _soln; }
        virtual void setSoln(StencilSolution* soln) { _soln = soln; }

        // Get foldablity.
        virtual int getNumFoldableDims() const {
            assert(_numFoldableDims >= 0);
            return _numFoldableDims;
        }
        virtual bool isFoldable() const {
            assert(_numFoldableDims >= 0);
            return _isFoldable;
        }

        // Get min and max observed indices.
        virtual const IntTuple& getMinIndices() const { return _minIndices; }
        virtual const IntTuple& getMaxIndices() const { return _maxIndices; }

        // Get the max sizes of halo across all steps for given pack.
        virtual IntTuple getHaloSizes(const string& packName, bool left) const {
            IntTuple halo;
            if (_halos.count(packName) && _halos.at(packName).count(left)) {
                for (auto i : _halos.at(packName).at(left)) {
                    auto& hs = i.second; // halo at step-val 'i'.
                    halo = halo.makeUnionWith(hs);
                    halo = halo.maxElements(hs, false);
                }
            }
            return halo;
        }

        // Get the max size in 'dim' of halo across all packs and steps.
        virtual int getHaloSize(const string& dim, bool left) const {
            int h = 0;
            for (auto& hi : _halos) {
                //auto& pname = hi.first;
                auto& h2 = hi.second;
                if (h2.count(left)) {
                    for (auto i : h2.at(left)) {
                        auto& hs = i.second; // halo at step-val 'i'.
                        auto* p = hs.lookup(dim);
                        if (p)
                            h = std::max(h, *p);
                    }
                }
            }
            return h;
        }

        // Determine whether dims are same.
        virtual bool areDimsSame(const Grid& other) const {
            if (_dims.size() != other._dims.size())
                return false;
            size_t i = 0;
            for (auto& dim : _dims) {
                auto d2 = other._dims[i].get();
                if (!dim->isSame(d2))
                    return false;
                i++;
            }
            return true;
        }

        // Determine how many values in step-dim are needed.
        virtual int getStepDimSize() const;

        // Determine whether grid can be folded.
        virtual void setFolding(const Dimensions& dims);

        // Determine whether halo sizes are equal.
        virtual bool isHaloSame(const Grid& other) const;

        // Update halos based on halo in 'other' grid.
        virtual void updateHalo(const Grid& other);

        // Update halos based on each value in 'offsets'.
        virtual void updateHalo(const string& packName, const IntTuple& offsets);

        // Update const indices based on 'indices'.
        virtual void updateConstIndices(const IntTuple& indices);

        // Create an expression to a specific point in this grid.
        // Note that this doesn't actually 'read' or 'write' a value;
        // it's just a node in an expression.
        virtual GridPointPtr makePoint(const NumExprPtrVec& args);
        virtual GridPointPtr makePoint() {
            NumExprPtrVec args;
            return makePoint(args);
        }

        // Convenience functions for zero dimensions (scalar).
        virtual operator NumExprPtr() { // implicit conversion.
            return makePoint();
        }
        virtual operator GridPointPtr() { // implicit conversion.
            return makePoint();
        }
        virtual GridPointPtr operator()() {
            return makePoint();
        }

        // Convenience functions for one dimension (array).
        virtual GridPointPtr operator[](const NumExprArg i1) {
            NumExprPtrVec args;
            args.push_back(i1);
            return makePoint(args);
        }
        virtual GridPointPtr operator()(const NumExprArg i1) {
            return operator[](i1);
        }

        // Convenience functions for more dimensions.
        virtual GridPointPtr operator()(const NumExprArg i1, const NumExprArg i2) {
            NumExprPtrVec args;
            args.push_back(i1);
            args.push_back(i2);
            return makePoint(args);
        }
        virtual GridPointPtr operator()(const NumExprArg i1, const NumExprArg i2,
                                        const NumExprArg i3) {
            NumExprPtrVec args;
            args.push_back(i1);
            args.push_back(i2);
            args.push_back(i3);
            return makePoint(args);
        }
        virtual GridPointPtr operator()(const NumExprArg i1, const NumExprArg i2,
                                        const NumExprArg i3, const NumExprArg i4) {
            NumExprPtrVec args;
            args.push_back(i1);
            args.push_back(i2);
            args.push_back(i3);
            args.push_back(i4);
            return makePoint(args);
        }
        virtual GridPointPtr operator()(const NumExprArg i1, const NumExprArg i2,
                                        const NumExprArg i3, const NumExprArg i4,
                                        const NumExprArg i5) {
            NumExprPtrVec args;
            args.push_back(i1);
            args.push_back(i2);
            args.push_back(i3);
            args.push_back(i4);
            args.push_back(i5);
            return makePoint(args);
        }
        virtual GridPointPtr operator()(const NumExprArg i1, const NumExprArg i2,
                                        const NumExprArg i3, const NumExprArg i4,
                                        const NumExprArg i5, const NumExprArg i6) {
            NumExprPtrVec args;
            args.push_back(i1);
            args.push_back(i2);
            args.push_back(i3);
            args.push_back(i4);
            args.push_back(i5);
            args.push_back(i6);
            return makePoint(args);
        }

        // APIs.
        virtual const string& get_name() const {
            return _name;
        }
        virtual int get_num_dims() const {
            return int(_dims.size());
        }
        virtual const string& get_dim_name(int n) const {
            assert(n >= 0);
            assert(n < get_num_dims());
            auto dp = _dims.at(n);
            assert(dp);
            return dp->getName();
        }
        virtual std::vector<std::string> get_dim_names() const;
        virtual bool
        is_dynamic_step_alloc() const {
            return !_isStepAllocFixed;
        }
        virtual void
        set_dynamic_step_alloc(bool enable) {
            _isStepAllocFixed = !enable;
        }
        virtual idx_t
        get_step_alloc_size() const {
            return getStepDimSize();
        }
        virtual void
        set_step_alloc_size(idx_t size) {
            _stepAlloc = size;
        }
        virtual yc_grid_point_node_ptr
        new_grid_point(const std::vector<yc_number_node_ptr>& index_exprs);
        virtual yc_grid_point_node_ptr
        new_grid_point(const std::initializer_list<yc_number_node_ptr>& index_exprs) {
            std::vector<yc_number_node_ptr> idx_expr_vec(index_exprs);
            return new_grid_point(idx_expr_vec);
        }
        virtual yc_grid_point_node_ptr
        new_relative_grid_point(const std::vector<int>& dim_offsets);
        virtual yc_grid_point_node_ptr
        new_relative_grid_point(const std::initializer_list<int>& dim_offsets) {
            std::vector<int> dim_ofs_vec(dim_offsets);
            return new_relative_grid_point(dim_ofs_vec);
        }
    };

    // A list of grids.  This holds pointers to grids defined by the stencil
    // class in the order in which they are added via the INIT_GRID_* macros.
    class Grids : public vector_set<Grid*> {
    public:

        Grids() {}
        virtual ~Grids() {}

        // Copy ctor.
        // Copies list of grid pointers, but not grids (shallow copy).
        Grids(const Grids& src) : vector_set<Grid*>(src) {}

        // Determine whether each grid can be folded.
        virtual void setFolding(const Dimensions& dims) {
            for (auto gp : *this)
                gp->setFolding(dims);
        }
    };

} // namespace yask.
