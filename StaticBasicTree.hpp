/*
 * Copyright (C) 2017 Robert Mueller
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact: Robert Mueller <romueller@techfak.uni-bielefeld.de>
 * Faculty of Technology, Bielefeld University,
 * PO box 100131, DE-33501 Bielefeld, Germany
 */

#ifndef K2TREES_STATICBASICTREE_HPP
#define K2TREES_STATICBASICTREE_HPP

#include <queue>

#include "K2Tree.hpp"
#include "Utility.hpp"

/**
 * Simplest implementation of K2Tree.
 *
 * Uses the same arity (k) on all levels and for both rows and columns.
 * The described relation matrix is quadratic with an edge length of nPrime,
 * where nPrime is the smallest power of k that exceeds the row / column numbers
 * of all relation pairs.
 */
template<typename E>
class BasicK2Tree : public virtual K2Tree<E> {

public:
    typedef E elem_type;

    typedef typename K2Tree<elem_type>::matrix_type matrix_type;
    typedef typename K2Tree<elem_type>::list_type list_type;
    typedef typename K2Tree<elem_type>::positions_type positions_type;
    typedef typename K2Tree<elem_type>::pairs_type pairs_type;


    BasicK2Tree() {
        // nothing to do
    }

    BasicK2Tree(const BasicK2Tree& other) {

        h_ = other.h_;
        k_ = other.k_;
        nPrime_ = other.nPrime_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

    }

    BasicK2Tree& operator=(const BasicK2Tree& other) {

        // check for self-assignment
        if (&other == this) {
            return *this;
        }

        h_ = other.h_;
        k_ = other.k_;
        nPrime_ = other.nPrime_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

        return *this;

    }

    /**
     * Matrix-based constructor (based on section 3.3.1. of Brisaboa et al.)
     *
     * Assumes that all rows of mat are equally long.
     */
    BasicK2Tree(const matrix_type& mat, const size_type k, const elem_type null = elem_type()) {

        null_ = null;

        k_ = k;
        h_ = std::max((size_type)1, logK(std::max(mat.size(), mat[0].size()), k_));
        nPrime_ = size_type(pow(k_, h_));

        std::vector<std::vector<bool>> levels(h_ - 1);
        buildFromMatrix(mat, levels, mat.size(), mat[0].size(), nPrime_, 1, 0, 0);

        size_type total = 0;
        for (size_type l = 0; l < h_ - 1; l++) {
            total += levels[l].size();
        }
        T_ = bit_vector_type(total);

        bit_vector_type::iterator outIter = T_.begin();
        for (size_type l = 0; l < h_ - 1; l++) {

            outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
            levels[l].clear();
            levels[l].shrink_to_fit();

        }

        R_ = rank_type(&T_);

    }

    /**
     * List-of-lists-based constructor (based on sections 3.3.2. - 3.3.4. of Brisaboa et al.)
     *
     * The actually used method depends on parameter mode.
     */
    BasicK2Tree(const std::vector<list_type>& lists, const size_type k, const int mode, const elem_type null = elem_type()) {

        null_ = null;

        size_type maxCol = 0;
        for (auto& row : lists) {
            for (auto& elem : row) {
                maxCol = std::max(maxCol, elem.first);
            }
        }

        k_ = k;
        h_ = std::max((size_type)1, logK(std::max(lists.size(), maxCol + 1), k_));
        nPrime_ = size_type(pow(k_, h_));

        switch (mode) {

            case 0: { // 3.3.2.

                std::vector<std::vector<bool>> levels(h_ - 1);
                std::vector<typename list_type::const_iterator> cursors;
                for (auto iter = lists.begin(); iter != lists.end(); iter++) {
                    cursors.push_back(iter->begin());
                }

                buildFromLists(lists, cursors, levels, nPrime_, 1, 0, 0);

                size_type total = 0;
                for (size_type l = 0; l < h_ - 1; l++) {
                    total += levels[l].size();
                }
                T_ = bit_vector_type(total);

                bit_vector_type::iterator outIter = T_.begin();
                for (size_type l = 0; l < h_ - 1; l++) {

                    outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
                    levels[l].clear();
                    levels[l].shrink_to_fit();

                }

                R_ = rank_type(&T_);

                break;

            }

            case 1: { // 3.3.3.

                buildFromListsViaTree(lists);

                R_ = rank_type(&T_);

                break;

            }

            case 2: { // 3.3.4.

                buildFromListsDynamicBitmaps(lists);

                break;

            }

            default: {

                buildFromListsDynamicBitmaps(lists);
                break;

            }

        }

    }

    /**
     * List-of-pairs-based constructor (based on section 3.3.5. of Brisaboa et al.)
     */
    BasicK2Tree(pairs_type& pairs, const size_type k, const elem_type null = elem_type()) {

        null_ = null;

        size_type maxIndex = 0;
        for (auto& p : pairs) {
            maxIndex = std::max({maxIndex, p.row, p.col});
        }

        k_ = k;
        h_ = std::max((size_type)1, logK(maxIndex + 1, k_));
        nPrime_ = size_type(pow(k_, h_));

        if (pairs.size() != 0) {
            buildFromListsInplace(pairs);
        }

        R_ = rank_type(&T_);

    }


    // returns the height of the K2Tree
    size_type getH() {
        return h_;
    }

    // returns the arity of the K2Tree
    size_type getK() {
        return k_;
    }

    size_type getNumRows() override {
        return nPrime_;
    }

    size_type getNumCols() override {
        return nPrime_;
    }

    elem_type getNull() override {
        return null_;
    }


    bool isNotNull(size_type i, size_type j) override {
        return checkInit(i, j);
    }

    elem_type getElement(size_type i, size_type j) override {
        return getInit(i, j);
    }

    std::vector<elem_type> getSuccessorElements(size_type i) override {

        std::vector<elem_type> succs;
        allSuccessorElementsIterative(succs, i);
//        successorsElemInit(succs, i);

        return succs;

    }

    std::vector<size_type> getSuccessorPositions(size_type i) override {

        std::vector<size_type> succs;
//        successorsPosInit(succs, i);
        allSuccessorPositionsIterative(succs, i);

        return succs;

    }

    pairs_type getSuccessorValuedPositions(size_type i) override {

        pairs_type succs;
        allSuccessorValuedPositionsIterative(succs, i);
//        successorsValPosInit(succs, i);

        return succs;

    }

    std::vector<elem_type> getPredecessorElements(size_type j) override {

        std::vector<elem_type> preds;
        predecessorsElemInit(preds, j);

        return preds;

    }

    std::vector<size_type> getPredecessorPositions(size_type j) override {

        std::vector<size_type> preds;
        predecessorsPosInit(preds, j);

        return preds;

    }

    pairs_type getPredecessorValuedPositions(size_type j) override {

        pairs_type preds;
        predecessorsValPosInit(preds, j);

        return preds;

    }

    std::vector<elem_type> getElementsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        std::vector<elem_type> elements;
        rangeElemInit(elements, i1, i2, j1, j2);
//        rangeElemInit(elements, i1, std::min(i2, nPrime_ - 1), j1, std::min(j2, nPrime_ - 1));

        return elements;

    }

    positions_type getPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        positions_type pairs;
        rangePosInit(pairs, i1, i2, j1, j2);
//        rangePosInit(pairs, i1, std::min(i2, nPrime_ - 1), j1, std::min(j2, nPrime_ - 1));

        return pairs;

    }

    pairs_type getValuedPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        pairs_type pairs;
        rangeValPosInit(pairs, i1, i2, j1, j2);
//        rangeValPosInit(pairs, i1, std::min(i2, nPrime_ - 1), j1, std::min(j2, nPrime_ - 1));

        return pairs;

    }

    std::vector<elem_type> getAllElements() override {
        return getElementsInRange(0, nPrime_ - 1, 0, nPrime_ - 1);
    }

    positions_type getAllPositions() override {
        return getPositionsInRange(0, nPrime_ - 1, 0, nPrime_ - 1);
    }

    pairs_type getAllValuedPositions() override {
        return getValuedPositionsInRange(0, nPrime_ - 1, 0, nPrime_ - 1);
    }

    bool containsElement(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return elemInRangeInit(i1, i2, j1, j2);
//        return elemInRangeInit(i1, std::min(i2, nPrime_ - 1), j1, std::min(j2, nPrime_ - 1));
    }

    size_type countElements() override {

        size_type cnt = 0;
        for (size_type i = 0; i < L_.size(); i++) {
            cnt += (L_[i] != null_);
        }

        return cnt;

    }


    BasicK2Tree* clone() const override {
        return new BasicK2Tree<elem_type>(*this);
    }

    void print(bool all = false) override {

        std::cout << "### Parameters ###" << std::endl;
        std::cout << "h  = " << h_ << std::endl;
        std::cout << "k  = " << k_ << std::endl;
        std::cout << "n' = " << nPrime_ << std::endl;
        std::cout << "null = " << null_ << std::endl;

        if (all) {

            std::cout << "### T ###" << std::endl;
            for (size_type i = 0; i < T_.size(); i++) std::cout << T_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### L ###" << std::endl;
            for (size_type i = 0; i < L_.size(); i++) std::cout << L_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### R ###" << std::endl;
            printRanks(R_);
            std::cout << std::endl;

        }

    }

    // note: can "invalidate" the data structure (containsLink() probably won't work correctly afterwards)
    void setNull(size_type i, size_type j) override {
        setInit(i, j);
    }

    size_type getFirstSuccessor(size_type i) override {
//        return firstSuccessorInit(i);
        return firstSuccessorPositionIterative(i);
    }


    /*
     * Method aliases using "relation nomenclature" (similar to the names proposed by Brisaboa et al.)
     */

    bool areRelated(size_type i, size_type j) override {
        return isNotNull(i, j);
    }

    std::vector<size_type> getSuccessors(size_type i) override {
        return getSuccessorPositions(i);
    }

    std::vector<size_type> getPredecessors(size_type j) override {
        return getPredecessorPositions(j);
    }

    positions_type getRange(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return getPositionsInRange(i1, i2, j1, j2);
    }

    bool containsLink(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return containsElement(i1, i2, j1, j2);
    }

    size_type countLinks() override {
        return countElements();
    }



private:
    // representation of all but the last levels of the K2Tree (internal structure)
    bit_vector_type T_;

    // representation of the last level of the K2Tree (actual values of the relation)
    std::vector<elem_type> L_;

    // rank data structure for navigation in T_
    rank_type R_;

    size_type h_; // height of the K2Tree
    size_type k_; // arity of the K2Tree
    size_type nPrime_; // edge length of the represented relation matrix

    elem_type null_; // null element


    /* helper method for construction from relation matrix */

    bool buildFromMatrix(const matrix_type& mat, std::vector<std::vector<bool>>& levels, size_type numRows, size_type numCols, size_type n, size_type l, size_type p, size_type q) {// 3.3.1 / Algorithm 1

        if (l == h_) {

            std::vector<elem_type> C;

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {
                    C.push_back((((p + i) < numRows) && ((q + j) < numCols)) ? mat[p + i][q + j] : null_);
                }
            }

            if (isAll(C, null_)) {
                return false;
            } else {

                L_.insert(L_.end(), C.begin(), C.end());
                return true;

            }

        } else {

            std::vector<bool> C;

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {
                    C.push_back(buildFromMatrix(mat, levels, numRows, numCols, n / k_, l + 1, p + i * (n / k_), q + j * (n / k_)));
                }
            }

            if (isAllZero(C)) {
                return false;
            } else {

                levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
                return true;

            }

        }

    }

    /* helper method for construction from relation lists */

    bool buildFromLists(const std::vector<list_type>& lists, std::vector<typename list_type::const_iterator>& cursors, std::vector<std::vector<bool>>& levels, size_type n, size_type l, size_type p, size_type q) {// 3.3.2

        if (l == h_) {

            std::vector<elem_type> C;

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {

                    C.push_back(((p + i) < lists.size()) && (cursors[p + i] != lists[p + i].end()) && ((q + j) == cursors[p + i]->first) ? cursors[p + i]->second : null_);
                    if (C.back()) cursors[p + i]++;

                }
            }

            if (isAll(C, null_)) {
                return false;
            } else {

                L_.insert(L_.end(), C.begin(), C.end());
                return true;

            }

        } else {

            std::vector<bool> C;

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {
                    C.push_back(buildFromLists(lists, cursors, levels, n / k_, l + 1, p + i * (n / k_), q + j * (n / k_)));
                }
            }

            if (isAllZero(C)) {
                return false;
            } else {

                levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
                return true;

            }

        }

    }

    /* helper methods for construction from relation lists via temporary tree */

    void buildFromListsViaTree(const std::vector<list_type>& lists) {// 3.3.3, so far without special bit vectors without initialisation

        Node<elem_type>* root = new Node<elem_type>(null_);

        for (size_type i = 0; i < lists.size(); i++) {
            for (size_type j = 0; j < lists[i].size(); j++) {
                insert(root, nPrime_, i, lists[i][j].first, lists[i][j].second);
            }
        }

        if (!root->isLeaf()) {

            std::vector<bool> T;

            // traverse over tree and generate T and L while doing it
            std::queue<Node<elem_type>*> queue;
            Node<elem_type>* node;
            Node<elem_type>* child;
            queue.push(root);

            while (!queue.empty()) {

                node = queue.front();
                queue.pop();

                for (size_type i = 0; i < k_ * k_; i++) {

                    child = node->getChild(i);

                    if (child != 0 && child->isLeaf()) {
                        L_.push_back(child->getLabel());
                    } else {

                        T.push_back(child != 0);

                        if (T.back()) {
                            queue.push(child);
                        }

                    }

                }

            }

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        delete root;

    }

    void insert(Node<elem_type>* node, size_type n, size_type p, size_type q, elem_type val) {

        if (n == k_) {

            if (node->isLeaf()) {
                node->turnInternal(k_ * k_, true);
            }

            node->addChild(p * k_ + q, val);

        } else {

            if (node->isLeaf()) {
                node->turnInternal(k_ * k_, false);
            }

            size_type z = (p / (n / k_)) * k_ + q / (n / k_);

            insert(node->hasChild(z) ? node->getChild(z) : node->addChild(z, null_), n / k_, p % (n / k_), q % (n / k_), val);

        }

    }

    /* helper methods for construction from relation lists via dynamic bitmap representations */

    void buildFromListsDynamicBitmaps(const std::vector<list_type>& lists) {// 3.3.4, currently no succinct dynamic bitmaps

        if (h_ == 1) {

            L_ = std::vector<elem_type>(k_ * k_, null_);

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    L_[i * k_ + lists[i][j].first] = lists[i][j].second;
                }
            }

            if (isAll(L_, null_)) {
                L_ = std::vector<elem_type>(0);
            }

        } else {

            std::vector<bool> T;
            NaiveDynamicRank R;

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    insertInit(T, R, i, lists[i][j].first, lists[i][j].second);
                }
            }

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        R_ = rank_type(&T_);

    }

    void insertInit(std::vector<bool>& T, NaiveDynamicRank& R, size_type p, size_type q, elem_type val) {

        if (T.empty()) {

            T = std::vector<bool>(k_ * k_);
            R = NaiveDynamicRank(T);

        }

        insert(T, R, nPrime_ / k_, p % (nPrime_ / k_), q % (nPrime_ / k_), val, (p / (nPrime_ / k_)) * k_ + q / (nPrime_ / k_), 1);

    }

    void insert(std::vector<bool>& T, NaiveDynamicRank& R, size_type n, size_type p, size_type q, elem_type val, size_type z, size_type l) {

        if (!T[z]) {

            T[z] = 1;
            R.increaseFrom(z + 1);

            size_type y = R.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_);

            if ((l + 1) == h_) {

                L_.insert(L_.begin() + R.rank(z + 1) * k_ * k_ - T.size(), k_ * k_, null_);
                L_[y - T.size()] = val;

            } else {

                T.insert(T.begin() + R.rank(z + 1) * k_ * k_, k_ * k_, 0);
                R.insert(R.rank(z + 1) * k_ * k_ + 1, k_ * k_);

                insert(T, R, n / k_, p % (n / k_), q % (n / k_), val, y, l + 1);

            }

        } else {

            size_type y = R.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_);

            if ((l + 1) == h_) {
                L_[y - T.size()] = val;
            } else {
                insert(T, R, n / k_, p % (n / k_), q % (n / k_), val, y, l + 1);
            }

        }

    }

    /* helper methods for inplace construction from single list of pairs */

    size_type computeKey(const typename pairs_type::value_type& pair, const Subproblem& sp, size_type width) {
        return ((pair.row - sp.firstRow) / width) * k_ + (pair.col - sp.firstCol) / width;
    }

    void countingSort(pairs_type& pairs, std::vector<std::pair<size_type, size_type>>& intervals, const Subproblem& sp, size_type width, size_type sup) {

        std::vector<size_type> counts(sup);

        // determine key frequencies
        for (size_type i = sp.left; i < sp.right; i++) {
            counts[computeKey(pairs[i], sp, width)]++;
        }

        // determine starting index for each key
        size_type total = 0;
        size_type tmp;

        for (size_type key = 0; key < sup; key++) {

            tmp = counts[key];
            counts[key] = total;
            total += tmp;

            intervals[key].first = counts[key];
            intervals[key].second = total;

        }

        // reorder pairs of current subproblem
        pairs_type tmpPairs(sp.right - sp.left + 1);
        for (size_type i = sp.left; i < sp.right; i++) {

            tmpPairs[counts[computeKey(pairs[i], sp, width)]] = pairs[i];
            counts[computeKey(pairs[i], sp, width)]++;

        }

        for (size_type i = sp.left; i < sp.right; i++) {
            pairs[i] = tmpPairs[i - sp.left];
        }

    }

    void buildFromListsInplace(pairs_type& pairs) {// 3.3.5

        std::queue<Subproblem> queue;
        Subproblem sp;
        size_type S;
        std::vector<std::pair<size_type, size_type>> intervals(k_ * k_);
        std::vector<bool> T;
        std::vector<elem_type> appToL;

        queue.push(Subproblem(0, nPrime_ - 1, 0, nPrime_ - 1, 0, pairs.size()));

        while (!queue.empty()) {

            sp = queue.front();
            queue.pop();

            S = sp.lastRow - sp.firstRow + 1;

            if (S > k_) {

                countingSort(pairs, intervals, sp, S / k_, k_ * k_);

                for (size_type i = 0; i < k_ * k_; i++) {

                    if (intervals[i].first < intervals[i].second) {

                        T.push_back(true);
                        queue.push(Subproblem(
                                sp.firstRow + (i / k_) * (S / k_),
                                sp.firstRow + (i / k_ + 1) * (S / k_) - 1,
                                sp.firstCol + (i % k_) * (S / k_),
                                sp.firstCol + (i % k_ + 1) * (S / k_) - 1,
                                sp.left + intervals[i].first,
                                sp.left + intervals[i].second
                        ));

                    } else {
                        T.push_back(false);
                    }

                }

            } else {

                appToL = std::vector<elem_type>(k_ * k_);

                for (size_type i = sp.left; i < sp.right; i++) {
                    appToL[(pairs[i].row - sp.firstRow) * k_ + (pairs[i].col - sp.firstCol)] = pairs[i].val;
                }

                L_.insert(L_.end(), appToL.begin(), appToL.end());

            }

        }

        T_ = bit_vector_type(T.size());
        std::move(T.begin(), T.end(), T_.begin());

    }



    /* isNotNull() */

    bool checkInit(size_type p, size_type q) {
        return (L_.empty()) ? false : check(nPrime_ / k_, p % (nPrime_ / k_), q % (nPrime_ / k_), (p / (nPrime_ / k_)) * k_ + q / (nPrime_ / k_));
    }

    bool check(size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            return (L_[z - T_.size()] != null_);
        } else {
            return T_[z] ? check(n / k_, p % (n / k_), q % (n / k_), R_.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_)) : false;
        }

    }

    /* getElement() */

    elem_type getInit(size_type p, size_type q) {
        return (L_.empty()) ? null_ : get(nPrime_ / k_, p % (nPrime_ / k_), q % (nPrime_ / k_), (p / (nPrime_ / k_)) * k_ + q / (nPrime_ / k_));
    }

    elem_type get(size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            return L_[z - T_.size()];
        } else {
            return T_[z] ? get(n / k_, p % (n / k_), q % (n / k_), R_.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_)) : null_;
        }

    }


    /* getSuccessorElements() */

    void allSuccessorElementsIterative(std::vector<elem_type>& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * nPrime_;
            for (size_type i = 0; i < nPrime_; i++) {
                if (L_[offset + i] != null_) {
                    succs.push_back(L_[offset + i]);
                }
            }

        } else {

            // successorsPosInit
            size_type n = nPrime_/ k_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = k_ * (relP / n); j < k_; j++, dq += n, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= n;
            n /= k_;
            for (; n > 1; relP %= n, n /= k_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n);

                        for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                        if (L_[y] != null_) {
                            succs.push_back(L_[y]);
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsElemInit(std::vector<elem_type>& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = k_ * (p / (nPrime_ / k_));

            for (size_type j = 0; j < k_; j++) {
                successorsElem(succs, nPrime_ / k_, p % (nPrime_ / k_), (nPrime_ / k_) * j, y + j);
            }

        }

    }

    void successorsElem(std::vector<elem_type>& succs, size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                succs.push_back(L_[z - T_.size()]);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + k_ * (p / (n / k_));

                for (size_type j = 0; j < k_; j++) {
                    successorsElem(succs, n / k_, p % (n / k_), q + (n / k_) * j, y + j);
                }

            }

        }


    }

    /* getSuccessorPositions() */

    void allSuccessorPositionsIterative(std::vector<size_type>& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * nPrime_;
            for (size_type i = 0; i < nPrime_; i++) {
                if (L_[offset + i] != null_) {
                    succs.push_back(i);
                }
            }

        } else {

            // successorsPosInit
            size_type n = nPrime_/ k_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = k_ * (relP / n); j < k_; j++, dq += n, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= n;
            n /= k_;
            for (; n > 1; relP %= n, n /= k_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n);

                        for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                        if (L_[y] != null_) {
                            succs.push_back(newDq);
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsPosInit(std::vector<size_type>& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = k_ * (p / (nPrime_ / k_));

            for (size_type j = 0; j < k_; j++) {
                successorsPos(succs, nPrime_ / k_, p % (nPrime_ / k_), (nPrime_ / k_) * j, y + j);
            }

        }

    }

    void successorsPos(std::vector<size_type>& succs, size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                succs.push_back(q);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + k_ * (p / (n / k_));

                for (size_type j = 0; j < k_; j++) {
                    successorsPos(succs, n / k_, p % (n / k_), q + (n / k_) * j, y + j);
                }

            }

        }


    }

    /* getSuccessorValuedPositions() */

    void allSuccessorValuedPositionsIterative(pairs_type& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * nPrime_;
            for (size_type i = 0; i < nPrime_; i++) {
                if (L_[offset + i] != null_) {
                    succs.push_back(ValuedPosition<elem_type>(p, i, L_[offset + i]));
                }
            }

        } else {

            // successorsPosInit
            size_type n = nPrime_/ k_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = k_ * (relP / n); j < k_; j++, dq += n, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= n;
            n /= k_;
            for (; n > 1; relP %= n, n /= k_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n);

                        for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                        if (L_[y] != null_) {
                            succs.push_back(ValuedPosition<elem_type>(p, newDq, L_[y]));
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsValPosInit(pairs_type& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = k_ * (p / (nPrime_ / k_));

            for (size_type j = 0; j < k_; j++) {
                successorsValPos(succs, nPrime_ / k_, p % (nPrime_ / k_), (nPrime_ / k_) * j, y + j);
            }

            for (auto& s : succs) {
                s.row = p;
            }

        }

    }

    void successorsValPos(pairs_type& succs, size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                succs.push_back(ValuedPosition<elem_type>(0, q, L_[z - T_.size()]));
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + k_ * (p / (n / k_));

                for (size_type j = 0; j < k_; j++) {
                    successorsValPos(succs, n / k_, p % (n / k_), q + (n / k_) * j, y + j);
                }

            }

        }


    }

    /* getFirstSuccessor() */

    size_type firstSuccessorPositionIterative(size_type p) {

        if (L_.empty()) return nPrime_;

        if (T_.size() == 0) {

            size_type offset = p * nPrime_;
            for (size_type i = 0; i < nPrime_; i++) {
                if (L_[offset + i] != null_) {
                    return i;
                }
            }

        } else {

            std::stack<ExtendedSubrowInfo> stack;
            stack.emplace(nPrime_ / k_, nPrime_ / k_, p % (nPrime_ / k_), 0, k_ * (p / (nPrime_ / k_)), 0);

            while (!stack.empty()) {

                auto& cur = stack.top();

                if (cur.j == k_) {
                    stack.pop();
                } else {

                    if (cur.z >= T_.size()) {

                        if (L_[cur.z - T_.size()] != null_) {
                            return cur.dq;
                        }

                    } else {

                        if (T_[cur.z]) {
                            stack.emplace(cur.nr / k_, cur.nc / k_, cur.p % (cur.nr / k_), cur.dq, R_.rank(cur.z + 1) * k_ * k_ + k_ * (cur.p / (cur.nr / k_)), 0);
                        }

                    }

                    cur.dq += cur.nc;
                    cur.z++;
                    cur.j++;

                }

            }
        }

        return nPrime_;

    }

    size_type firstSuccessorInit(size_type p) {

        size_type pos = nPrime_;

        if (!L_.empty()) {

            size_type y = k_ * (p / (nPrime_ / k_));

            for (size_type j = 0; j < k_ && pos == nPrime_; j++) {
                pos = firstSuccessor(nPrime_ / k_, p % (nPrime_ / k_), (nPrime_ / k_) * j, y + j);
            }

        }

        return pos;

    }

    size_type firstSuccessor(size_type n, size_type p, size_type q, size_type z) {

        size_type pos = nPrime_;

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                pos = q;
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + k_ * (p / (n / k_));

                for (size_type j = 0; j < k_ && pos == nPrime_; j++) {
                    pos = firstSuccessor(n / k_, p % (n / k_), q + (n / k_) * j, y + j);
                }

            }

        }

        return pos;

    }


    /* getPredecessorElements() */

    void predecessorsElemInit(std::vector<elem_type>& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (nPrime_ / k_);

            for (size_type i = 0; i < k_; i++) {
                predecessorsElem(preds, nPrime_ / k_, q % (nPrime_ / k_), (nPrime_ / k_) * i, y + i * k_);
            }

        }

    }

    void predecessorsElem(std::vector<elem_type>& preds, size_type n, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                preds.push_back(L_[z - T_.size()]);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + q / (n / k_);

                for (size_type i = 0; i < k_; i++) {
                    predecessorsElem(preds, n / k_, q % (n / k_), p + (n / k_) * i, y + i * k_);
                }

            }

        }

    }

    /* getPredecessorPositions() */

    void predecessorsPosInit(std::vector<size_type>& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (nPrime_ / k_);

            for (size_type i = 0; i < k_; i++) {
                predecessorsPos(preds, nPrime_ / k_, q % (nPrime_ / k_), (nPrime_ / k_) * i, y + i * k_);
            }

        }

    }

    void predecessorsPos(std::vector<size_type>& preds, size_type n, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                preds.push_back(p);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + q / (n / k_);

                for (size_type i = 0; i < k_; i++) {
                    predecessorsPos(preds, n / k_, q % (n / k_), p + (n / k_) * i, y + i * k_);
                }

            }

        }

    }

    /* getPredecessorValuedPositions() */

    void predecessorsValPosInit(pairs_type& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (nPrime_ / k_);

            for (size_type i = 0; i < k_; i++) {
                predecessorsValPos(preds, nPrime_ / k_, q % (nPrime_ / k_), (nPrime_ / k_) * i, y + i * k_);
            }

            for (auto& p : preds) {
                p.col = q;
            }

        }

    }

    void predecessorsValPos(pairs_type& preds, size_type n, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                preds.push_back(ValuedPosition<elem_type>(p, 0, L_[z - T_.size()]));
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + q / (n / k_);

                for (size_type i = 0; i < k_; i++) {
                    predecessorsValPos(preds, n / k_, q % (n / k_), p + (n / k_) * i, y + i * k_);
                }

            }

        }

    }


    /* getElementsInRange() */

    void rangeElemInit(std::vector<elem_type>& elements, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (size_type i = p1 / (nPrime_ / k_); i <= p2 / (nPrime_ / k_); i++) {

                p1Prime = (i == p1 / (nPrime_ / k_)) * (p1 % (nPrime_ / k_));
                p2Prime = (i == p2 / (nPrime_ / k_)) ? p2 % (nPrime_ / k_) : (nPrime_ / k_) - 1;

                for (size_type j = q1 / (nPrime_ / k_); j <= q2 / (nPrime_ / k_); j++) {
                    rangeElem(
                            elements,
                            nPrime_ / k_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (nPrime_ / k_)) * (q1 % (nPrime_ / k_)),
                            (j == q2 / (nPrime_ / k_)) ? q2 % (nPrime_ / k_) : (nPrime_ / k_) - 1,
                            (nPrime_ / k_) * i,
                            (nPrime_ / k_) * j,
                            k_ * i + j
                    );
                }

            }

        }

    }

    void rangeElem(std::vector<elem_type>& elements, size_type n, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                elements.push_back(L_[z - T_.size()]);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_;
                size_type p1Prime, p2Prime;

                for (size_type i = p1 / (n / k_); i <= p2 / (n / k_); i++) {

                    p1Prime = (i == p1 / (n / k_)) * (p1 % (n / k_));
                    p2Prime = (i == p2 / (n / k_)) ? p2 % (n / k_) : n / k_ - 1;

                    for (size_type j = q1 / (n / k_); j <= q2 / (n / k_); j++) {
                        rangeElem(
                                elements,
                                n / k_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (n / k_)) * (q1 % (n / k_)),
                                (j == q2 / (n / k_)) ? q2 % (n / k_) : n / k_ - 1,
                                dp + (n / k_) * i,
                                dq + (n / k_) * j,
                                y + k_ * i + j
                        );
                    }

                }

            }

        }

    }

    /* getPositionsInRange() */

    void rangePosInit(positions_type& pairs, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (size_type i = p1 / (nPrime_ / k_); i <= p2 / (nPrime_ / k_); i++) {

                p1Prime = (i == p1 / (nPrime_ / k_)) * (p1 % (nPrime_ / k_));
                p2Prime = (i == p2 / (nPrime_ / k_)) ? p2 % (nPrime_ / k_) : (nPrime_ / k_) - 1;

                for (size_type j = q1 / (nPrime_ / k_); j <= q2 / (nPrime_ / k_); j++) {
                    rangePos(
                            pairs,
                            nPrime_ / k_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (nPrime_ / k_)) * (q1 % (nPrime_ / k_)),
                            (j == q2 / (nPrime_ / k_)) ? q2 % (nPrime_ / k_) : (nPrime_ / k_) - 1,
                            (nPrime_ / k_) * i,
                            (nPrime_ / k_) * j,
                            k_ * i + j
                    );
                }

            }

        }

    }

    void rangePos(positions_type& pairs, size_type n, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                pairs.push_back(std::make_pair(dp, dq));
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_;
                size_type p1Prime, p2Prime;

                for (size_type i = p1 / (n / k_); i <= p2 / (n / k_); i++) {

                    p1Prime = (i == p1 / (n / k_)) * (p1 % (n / k_));
                    p2Prime = (i == p2 / (n / k_)) ? p2 % (n / k_) : n / k_ - 1;

                    for (size_type j = q1 / (n / k_); j <= q2 / (n / k_); j++) {
                        rangePos(
                                pairs,
                                n / k_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (n / k_)) * (q1 % (n / k_)),
                                (j == q2 / (n / k_)) ? q2 % (n / k_) : n / k_ - 1,
                                dp + (n / k_) * i,
                                dq + (n / k_) * j,
                                y + k_ * i + j
                        );
                    }

                }

            }

        }

    }

    /* getValuedPositionsRange() */

    void rangeValPosInit(pairs_type& pairs, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (size_type i = p1 / (nPrime_ / k_); i <= p2 / (nPrime_ / k_); i++) {

                p1Prime = (i == p1 / (nPrime_ / k_)) * (p1 % (nPrime_ / k_));
                p2Prime = (i == p2 / (nPrime_ / k_)) ? p2 % (nPrime_ / k_) : (nPrime_ / k_) - 1;

                for (size_type j = q1 / (nPrime_ / k_); j <= q2 / (nPrime_ / k_); j++) {
                    rangeValPos(
                            pairs,
                            nPrime_ / k_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (nPrime_ / k_)) * (q1 % (nPrime_ / k_)),
                            (j == q2 / (nPrime_ / k_)) ? q2 % (nPrime_ / k_) : (nPrime_ / k_) - 1,
                            (nPrime_ / k_) * i,
                            (nPrime_ / k_) * j,
                            k_ * i + j
                    );
                }

            }

        }

    }

    void rangeValPos(pairs_type& pairs, size_type n, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()] != null_) {
                pairs.push_back(ValuedPosition<elem_type>(dp, dq, L_[z - T_.size()]));
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_;
                size_type p1Prime, p2Prime;

                for (size_type i = p1 / (n / k_); i <= p2 / (n / k_); i++) {

                    p1Prime = (i == p1 / (n / k_)) * (p1 % (n / k_));
                    p2Prime = (i == p2 / (n / k_)) ? p2 % (n / k_) : n / k_ - 1;

                    for (size_type j = q1 / (n / k_); j <= q2 / (n / k_); j++) {
                        rangeValPos(
                                pairs,
                                n / k_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (n / k_)) * (q1 % (n / k_)),
                                (j == q2 / (n / k_)) ? q2 % (n / k_) : n / k_ - 1,
                                dp + (n / k_) * i,
                                dq + (n / k_) * j,
                                y + k_ * i + j
                        );
                    }

                }

            }

        }

    }


    /* containsElement() */

    bool elemInRangeInit(size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            // dividing by k_ (as stated in the paper) in not correct,
            // because it does not use the size of the currently considered submatrix but of its submatrices
            if ((p1 == 0) && (q1 == 0) && (p2 == (nPrime_ /*/ k_*/ - 1)) && (q2 == (nPrime_ /*/ k_*/ - 1))) {
                return 1;
            }

            size_type p1Prime, p2Prime;

            for (size_type i = p1 / (nPrime_ / k_); i <= p2 / (nPrime_ / k_); i++) {

                p1Prime = (i == p1 / (nPrime_ / k_)) * (p1 % (nPrime_ / k_));
                p2Prime = (i == p2 / (nPrime_ / k_)) ? (p2 % (nPrime_ / k_)) : nPrime_ / k_ - 1;

                for (size_type j = q1 / (nPrime_ / k_); j <= q2 / (nPrime_ / k_); j++) {

                    if (elemInRange(nPrime_ / k_, p1Prime, p2Prime, (j == q1 / (nPrime_ / k_)) * (q1 % (nPrime_ / k_)), (j == q2 / (nPrime_ / k_)) ? q2 % (nPrime_ / k_) : nPrime_ / k_ - 1, k_ * i + j)) {
                        return true;
                    }

                }

            }

        }

        return false;

    }

    bool elemInRange(size_type n, size_type p1, size_type p2, size_type q1, size_type q2, size_type z) {

        if (z >= T_.size()) {

            return L_[z - T_.size()];

        } else {

            if (T_[z]) {

                // dividing by k_ (as stated in the paper) in not correct,
                // because it does not use the size of the currently considered submatrix but of its submatrices
                if ((p1 == 0) && (q1 == 0) && (p2 == (n /*/ k_*/ - 1)) && (q2 == (n /*/ k_*/ - 1))) {
                    return true;
                }

                size_type y = R_.rank(z + 1) * k_ * k_;
                size_type p1Prime, p2Prime;

                for (size_type i = p1 / (n / k_); i <= p2 / (n / k_); i++) {

                    p1Prime = (i == p1 / (n / k_)) * (p1 % (n / k_));
                    p2Prime = (i == p2 / (n / k_)) ? (p2 % (n / k_)) : n / k_ - 1;

                    for (size_type j = q1 / (n / k_); j <= q2 / (n / k_); j++) {

                        if (elemInRange(n / k_, p1Prime, p2Prime, (j == q1 / (n / k_)) * (q1 % (n / k_)), (j == q2 / (n / k_)) ? q2 % (n / k_) : n / k_ - 1, y + k_ * i + j)) {
                            return true;
                        }

                    }

                }

            }

            return false;

        }

    }


    /* setNull() */

    void setInit(size_type p, size_type q) {

        if (!L_.empty()) {
            set(nPrime_ / k_, p % (nPrime_ / k_), q % (nPrime_ / k_), (p / (nPrime_ / k_)) * k_ + q / (nPrime_ / k_));
        }

    }

    void set(size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            L_[z - T_.size()] = null_;
        } else {

            if (T_[z]) {
                set(n / k_, p % (n / k_), q % (n / k_), R_.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_));
            }

        }

    }

};


/**
 * Bool specialisation of BasicK2Tree.
 *
 * Has the same characteristics as the general implementation above,
 * but makes use of some simplifications since the only non-null value is 1 / true.
 */
template<>
class BasicK2Tree<bool> : virtual public K2Tree<bool> {

public:
    typedef bool elem_type;

    typedef K2Tree<elem_type>::matrix_type matrix_type;
    typedef K2Tree<elem_type>::list_type list_type;
    typedef K2Tree<elem_type>::positions_type positions_type;
    typedef K2Tree<elem_type>::pairs_type pairs_type;


    BasicK2Tree() {
        // nothing to do
    }

    BasicK2Tree(const BasicK2Tree& other) {

        h_ = other.h_;
        k_ = other.k_;
        nPrime_ = other.nPrime_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

    }

    BasicK2Tree& operator=(const BasicK2Tree& other) {

        // check for self-assignment
        if (&other == this) {
            return *this;
        }

        h_ = other.h_;
        k_ = other.k_;
        nPrime_ = other.nPrime_;
        null_ = other.null_;

        T_ = other.T_;
        L_ = other.L_;
        R_ = rank_type(&T_);

        return *this;

    }

    /**
     * Matrix-based constructor (based on section 3.3.1. of Brisaboa et al.)
     *
     * Assumes that all rows of mat are equally long.
     */
    BasicK2Tree(const matrix_type& mat, const size_type k) {

        null_ = false;

        k_ = k;
        h_ = std::max((size_type)1, logK(std::max(mat.size(), mat[0].size()), k_));
        nPrime_ = size_type(pow(k_, h_));

        std::vector<std::vector<bool>> levels(h_);
        buildFromMatrix(mat, levels, mat.size(), mat[0].size(), nPrime_, 1, 0, 0);

        size_type total = 0;
        for (size_type l = 0; l < h_ - 1; l++) {
            total += levels[l].size();
        }
        T_ = bit_vector_type(total);

        bit_vector_type::iterator outIter = T_.begin();
        for (size_type l = 0; l < h_ - 1; l++) {

            outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
            levels[l].clear();
            levels[l].shrink_to_fit();

        }

        L_ = bit_vector_type(levels[h_ - 1].size());
        std::move(levels[h_ - 1].begin(), levels[h_ - 1].end(), L_.begin());
        levels[h_ - 1].clear();
        levels[h_ - 1].shrink_to_fit();

        R_ = rank_type(&T_);

    }

    /**
     * List-of-lists-based constructor (based on sections 3.3.2. - 3.3.4. of Brisaboa et al.)
     *
     * The actually used method depends on parameter mode.
     */
    BasicK2Tree(const RelationLists& lists, const size_type k, const int mode) {

        null_ = false;

        size_type maxCol = 0;
        for (auto& row : lists) {
            for (auto& elem : row) {
                maxCol = std::max(maxCol, elem);
            }
        }

        k_ = k;
        h_ = std::max((size_type)1, logK(std::max(lists.size(), maxCol + 1), k_));
        nPrime_ = size_type(pow(k_, h_));

        switch (mode) {

            case 0: { // 3.3.2.

                std::vector<std::vector<bool>> levels(h_);
                std::vector<RelationList::const_iterator> cursors;
                for (auto iter = lists.begin(); iter != lists.end(); iter++) {
                    cursors.push_back(iter->begin());
                }

                buildFromLists(lists, cursors, levels, nPrime_, 1, 0, 0);

                size_type total = 0;
                for (size_type l = 0; l < h_ - 1; l++) {
                    total += levels[l].size();
                }
                T_ = bit_vector_type(total);

                bit_vector_type::iterator outIter = T_.begin();
                for (size_type l = 0; l < h_ - 1; l++) {

                    outIter = std::move(levels[l].begin(), levels[l].end(), outIter);
                    levels[l].clear();
                    levels[l].shrink_to_fit();

                }

                L_ = bit_vector_type(levels[h_ - 1].size());
                std::move(levels[h_ - 1].begin(), levels[h_ - 1].end(), L_.begin());
                levels[h_ - 1].clear();
                levels[h_ - 1].shrink_to_fit();

                R_ = rank_type(&T_);

                break;

            }

            case 1: { // 3.3.3.

                buildFromListsViaTree(lists);

                R_ = rank_type(&T_);

                break;

            }

            case 2: { // 3.3.4.

                buildFromListsDynamicBitmaps(lists);

                break;

            }

            default: {

                buildFromListsDynamicBitmaps(lists);
                break;

            }

        }

    }

    /**
     * List-of-pairs-based constructor (based on section 3.3.5. of Brisaboa et al.)
     */
    BasicK2Tree(positions_type& pairs, const size_type k) {

        null_ = false;

        size_type maxIndex = 0;
        for (auto& p : pairs) {
            maxIndex = std::max({maxIndex, p.first, p.second});
        }

        k_ = k;
        h_ = std::max((size_type)1, logK(maxIndex + 1, k_));
        nPrime_ = size_type(pow(k_, h_));

        if (pairs.size() != 0) {
            buildFromListsInplace(pairs);
        }

        R_ = rank_type(&T_);

    }


    // returns the height of the K2Tree
    size_type getH() {
        return h_;
    }

    // returns the arity of the K2Tree
    size_type getK() {
        return k_;
    }

    size_type getNumRows() override {
        return nPrime_;
    }

    size_type getNumCols() override {
        return nPrime_;
    }

    elem_type getNull() override {
        return null_;
    }


    bool areRelated(size_type i, size_type j) override {
        return checkLinkInit(i, j);
    }

    std::vector<size_type> getSuccessors(size_type i) override {

        std::vector<size_type> succs;
//        successorsInit(succs, i);
        allSuccessorPositionsIterative(succs, i);

        return succs;

    }

    std::vector<size_type> getPredecessors(size_type j) override {

        std::vector<size_type> preds;
        predecessorsInit(preds, j);

        return preds;

    }

    positions_type getRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        positions_type pairs;
        rangeInit(pairs, i1, i2, j1, j2);
//        rangeInit(pairs, i1, std::min(i2, nPrime_ - 1), j1, std::min(j2, nPrime_ - 1));

        return pairs;

    }

    bool containsLink(size_type i1, size_type i2, size_type j1, size_type j2) {
        return linkInRangeInit(i1, i2, j1, j2);
//        return linkInRangeInit(i1, std::min(i2, nPrime_ - 1), j1, std::min(j2, nPrime_ - 1));
    }

    size_type countLinks() override {

        size_type res = 0;
        for (size_type i = 0; i < L_.size(); i++) {
            res += L_[i];
        }

        return res;

    }


    /*
     * General methods for completeness' sake (are redundant / useless for bool)
     */

    bool isNotNull(size_type i, size_type j) override {
        return areRelated(i, j);
    }

    elem_type getElement(size_type i, size_type j) override {
        return areRelated(i, j);
    }

    std::vector<elem_type> getSuccessorElements(size_type i) override {
        return std::vector<elem_type>(getSuccessors(i).size(), true);
    }

    std::vector<size_type> getSuccessorPositions(size_type i) override {
        return getSuccessors(i);
    }

    pairs_type getSuccessorValuedPositions(size_type i) override {

        auto pos = getSuccessors(i);

        pairs_type succs;
        for (auto j : pos) {
            succs.push_back(ValuedPosition<elem_type>(i, j, true));
        }

        return succs;

    }

    std::vector<elem_type> getPredecessorElements(size_type j) override {
        return std::vector<elem_type>(getPredecessors(j).size(), true);
    }

    std::vector<size_type> getPredecessorPositions(size_type j) override {
        return getPredecessors(j);
    }

    pairs_type getPredecessorValuedPositions(size_type j) override {

        auto pos = getPredecessors(j);

        pairs_type preds;
        for (auto i : pos) {
            preds.push_back(ValuedPosition<elem_type>(i, j, true));
        }

        return preds;

    }

    std::vector<elem_type> getElementsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return std::vector<elem_type>(getRange(i1, i2, j1, j2).size(), true);
    }

    positions_type getPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return getRange(i1, i2, j1, j2);
    }

    pairs_type getValuedPositionsInRange(size_type i1, size_type i2, size_type j1, size_type j2) override {

        auto pos = getRange(i1, i2, j1, j2);

        pairs_type pairs;
        for (auto& p : pos) {
            pairs.push_back(ValuedPosition<elem_type>(p.first, p.second, true));
        }

        return pairs;

    }

    std::vector<elem_type> getAllElements() override {
        return std::vector<elem_type>(countLinks(), true);
    }

    positions_type getAllPositions() override {
        return getRange(0, nPrime_ - 1, 0, nPrime_ - 1);
    }

    pairs_type getAllValuedPositions() override {

        auto pos = getAllPositions();

        pairs_type pairs;
        for (auto& p : pos) {
            pairs.push_back(ValuedPosition<elem_type>(p.first, p.second, true));
        }

        return pairs;

    }

    bool containsElement(size_type i1, size_type i2, size_type j1, size_type j2) override {
        return linkInRangeInit(i1, i2, j1, j2);
//        return linkInRangeInit(i1, std::min(i2, nPrime_ - 1), j1, std::min(j2, nPrime_ - 1));
    }

    size_type countElements() override {
        return countLinks();
    }


    BasicK2Tree* clone() const override {
        return new BasicK2Tree<elem_type>(*this);
    }

    void print(bool all = false) override {

        std::cout << "### Parameters ###" << std::endl;
        std::cout << "h  = " << h_ << std::endl;
        std::cout << "k  = " << k_ << std::endl;
        std::cout << "n' = " << nPrime_ << std::endl;
        std::cout << "null = " << null_ << std::endl;

        if (all) {

            std::cout << "### T ###" << std::endl;
            for (size_type i = 0; i < T_.size(); i++) std::cout << T_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### L ###" << std::endl;
            for (size_type i = 0; i < L_.size(); i++) std::cout << L_[i];
            std::cout << std::endl << std::endl;

            std::cout << "### R ###" << std::endl;
            printRanks(R_);
            std::cout << std::endl;

        }

    }

    // note: can "invalidate" the data structure (containsLink() probably won't work correctly afterwards)
    void setNull(size_type i, size_type j) override {
        setInit(i, j);
    }

    size_type getFirstSuccessor(size_type i) override {
//        return firstSuccessorInit(i);
        return firstSuccessorPositionIterative(i);
    }



private:
    // representation of all but the last levels of the K2Tree (internal structure)
    bit_vector_type T_;

    // representation of the last level of the K2Tree (actual values of the relation)
    bit_vector_type L_;

    // rank data structure for navigation in T_
    rank_type R_;

    size_type h_; // height of the K2Tree
    size_type k_; // arity of the K2Tree
    size_type nPrime_; // edge length of the represented relation matrix

    elem_type null_; // null element


    /* helper method for construction from relation matrix */

    bool buildFromMatrix(const RelationMatrix& mat, std::vector<std::vector<bool>>& levels, size_type numRows, size_type numCols, size_type n, size_type l, size_type p, size_type q) {// 3.3.1 / Algorithm 1

        std::vector<bool> C;

        if (l == h_) {

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {
                    C.push_back((((p + i) < numRows) && ((q + j) < numCols)) ? mat[p + i][q + j] : false);
                }
            }

        } else {

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {
                    C.push_back(buildFromMatrix(mat, levels, numRows, numCols, n / k_, l + 1, p + i * (n / k_), q + j * (n / k_)));
                }
            }

        }


        if (isAllZero(C)) {
            return false;
        } else {

            levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
            return true;

        }

    }

    /* helper method for construction from relation lists */

    bool buildFromLists(const RelationLists& lists, std::vector<RelationList::const_iterator>& cursors, std::vector<std::vector<bool>>& levels, size_type n, size_type l, size_type p, size_type q) {// 3.3.2

        std::vector<bool> C;

        if (l == h_) {

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {

                    C.push_back(((p + i) < lists.size()) && (cursors[p + i] != lists[p + i].end()) && ((q + j) == *(cursors[p + i])));
                    if (C.back()) cursors[p + i]++;

                }
            }

        } else {

            for (size_type i = 0; i < k_; i++) {
                for (size_type j = 0; j < k_; j++) {
                    C.push_back(buildFromLists(lists, cursors, levels, n / k_, l + 1, p + i * (n / k_), q + j * (n / k_)));
                }
            }

        }

        if (isAllZero(C)) {
            return false;
        } else {

            levels[l - 1].insert(levels[l - 1].end(), C.begin(), C.end());
            return true;

        }

    }

    /* helper methods for construction from relation lists via temporary tree */

    void buildFromListsViaTree(const RelationLists& lists) {// 3.3.3, so far without special bit vectors without initialisation

        Node<bool>* root = new Node<bool>(false);

        for (size_type i = 0; i < lists.size(); i++) {
            for (size_type j = 0; j < lists[i].size(); j++) {
                insert(root, nPrime_, i, lists[i][j]);
            }
        }

        if (!root->isLeaf()) {

            std::vector<bool> T, L;

            // traverse over tree and generate T and L while doing it
            std::queue<Node<bool>*> queue;
            Node<bool>* node;
            Node<bool>* child;
            queue.push(root);

            while (!queue.empty()) {

                node = queue.front();
                queue.pop();

                for (size_type i = 0; i < k_ * k_; i++) {

                    child = node->getChild(i);

                    if (child != 0 && child->isLeaf()) {
                        L.push_back(child->getLabel());
                    } else {

                        T.push_back(child != 0);

                        if (T.back()) {
                            queue.push(child);
                        }

                    }

                }

            }

            L_ = bit_vector_type(L.size());
            std::move(L.begin(), L.end(), L_.begin());
            L.clear();
            L.shrink_to_fit();

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        delete root;

    }

    void insert(Node<bool>* node, size_type n, size_type p, size_type q) {

        if (n == k_) {

            if (node->isLeaf()) {
                node->turnInternal(k_ * k_, true);
            }

            node->addChild(p * k_ + q, true);

        } else {

            if (node->isLeaf()) {
                node->turnInternal(k_ * k_, false);
            }

            size_type z = (p / (n / k_)) * k_ + q / (n / k_);

            insert(node->hasChild(z) ? node->getChild(z) : node->addChild(z, true), n / k_, p % (n / k_), q % (n / k_));

        }

    }

    /* helper methods for construction from relation lists via dynamic bitmap representations */

    void buildFromListsDynamicBitmaps(const RelationLists& lists) {// 3.3.4, currently no succinct dynamic bitmaps

        if (h_ == 1) {

            L_ = bit_vector_type(k_ * k_, 0);

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    L_[i * k_ + lists[i][j]] = 1;
                }
            }

            if (isAllZero(L_)) {
                L_ = bit_vector_type(0);
            }

        } else {

            std::vector<bool> T, L;
            NaiveDynamicRank R;

            for (size_type i = 0; i < lists.size(); i++) {
                for (size_type j = 0; j < lists[i].size(); j++) {
                    insertInit(T, L, R, i, lists[i][j]);
                }
            }

            L_ = bit_vector_type(L.size());
            std::move(L.begin(), L.end(), L_.begin());
            L.clear();
            L.shrink_to_fit();

            T_ = bit_vector_type(T.size());
            std::move(T.begin(), T.end(), T_.begin());

        }

        R_ = rank_type(&T_);

    }

    void insertInit(std::vector<bool>& T, std::vector<bool>& L, NaiveDynamicRank& R, size_type p, size_type q) {

        if (T.empty()) {

            T = std::vector<bool>(k_ * k_);
            R = NaiveDynamicRank(T);

        }

        insert(T, L, R, nPrime_ / k_, p % (nPrime_ / k_), q % (nPrime_ / k_), (p / (nPrime_ / k_)) * k_ + q / (nPrime_ / k_), 1);

    }

    void insert(std::vector<bool>& T, std::vector<bool>& L, NaiveDynamicRank& R, size_type n, size_type p, size_type q, size_type z, size_type l) {

        if (!T[z]) {

            T[z] = 1;
            R.increaseFrom(z + 1);

            size_type y = R.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_);

            if ((l + 1) == h_) {

                L.insert(L.begin() + R.rank(z + 1) * k_ * k_ - T.size(), k_ * k_, 0);
                L[y - T.size()] = 1;

            } else {

                T.insert(T.begin() + R.rank(z + 1) * k_ * k_, k_ * k_, 0);
                R.insert(R.rank(z + 1) * k_ * k_ + 1, k_ * k_);

                insert(T, L, R, n / k_, p % (n / k_), q % (n / k_), y, l + 1);

            }

        } else {

            size_type y = R.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_);

            if ((l + 1) == h_) {
                L[y - T.size()] = 1;
            } else {
                insert(T, L, R, n / k_, p % (n / k_), q % (n / k_), y, l + 1);
            }

        }

    }

    /* helper methods for inplace construction from single list of pairs */

    size_type computeKey(const positions_type::value_type& pair, const Subproblem& sp, size_type width) {
        return ((pair.first - sp.firstRow) / width) * k_ + (pair.second - sp.firstCol) / width;
    }

    void countingSort(positions_type& pairs, std::vector<std::pair<size_type, size_type>>& intervals, const Subproblem& sp, size_type width, size_type sup) {

        std::vector<size_type> counts(sup);

        // determine key frequencies
        for (size_type i = sp.left; i < sp.right; i++) {
            counts[computeKey(pairs[i], sp, width)]++;
        }

        // determine starting index for each key
        size_type total = 0;
        size_type tmp;

        for (size_type key = 0; key < sup; key++) {

            tmp = counts[key];
            counts[key] = total;
            total += tmp;

            intervals[key].first = counts[key];
            intervals[key].second = total;

        }

        // reorder pairs of current subproblem
        positions_type tmpPairs(sp.right - sp.left + 1);
        for (size_type i = sp.left; i < sp.right; i++) {

            tmpPairs[counts[computeKey(pairs[i], sp, width)]] = pairs[i];
            counts[computeKey(pairs[i], sp, width)]++;

        }

        for (size_type i = sp.left; i < sp.right; i++) {
            pairs[i] = tmpPairs[i - sp.left];
        }

    }

    void buildFromListsInplace(positions_type& pairs) {// 3.3.5

        std::queue<Subproblem> queue;
        Subproblem sp;
        size_type S;
        std::vector<std::pair<size_type, size_type>> intervals(k_ * k_);
        std::vector<bool> T, L;
        bit_vector_type appToL;

        queue.push(Subproblem(0, nPrime_ - 1, 0, nPrime_ - 1, 0, pairs.size()));

        while (!queue.empty()) {

            sp = queue.front();
            queue.pop();

            S = sp.lastRow - sp.firstRow + 1;

            if (S > k_) {

                countingSort(pairs, intervals, sp, S / k_, k_ * k_);

                for (size_type i = 0; i < k_ * k_; i++) {

                    if (intervals[i].first < intervals[i].second) {

                        T.push_back(true);
                        queue.push(Subproblem(
                                sp.firstRow + (i / k_) * (S / k_),
                                sp.firstRow + (i / k_ + 1) * (S / k_) - 1,
                                sp.firstCol + (i % k_) * (S / k_),
                                sp.firstCol + (i % k_ + 1) * (S / k_) - 1,
                                sp.left + intervals[i].first,
                                sp.left + intervals[i].second
                        ));

                    } else {
                        T.push_back(false);
                    }

                }

            } else {

                appToL = bit_vector_type(k_ * k_);

                for (size_type i = sp.left; i < sp.right; i++) {
                    appToL[(pairs[i].first - sp.firstRow) * k_ + (pairs[i].second - sp.firstCol)] = true;
                }

                L.insert(L.end(), appToL.begin(), appToL.end());

            }

        }

        L_ = bit_vector_type(L.size());
        std::move(L.begin(), L.end(), L_.begin());
        L.clear();
        L.shrink_to_fit();

        T_ = bit_vector_type(T.size());
        std::move(T.begin(), T.end(), T_.begin());

    }


    /* areRelated() */

    bool checkLinkInit(size_type p, size_type q) {
        return (L_.empty()) ? false : checkLink(nPrime_ / k_, p % (nPrime_ / k_), q % (nPrime_ / k_), (p / (nPrime_ / k_)) * k_ + q / (nPrime_ / k_));
    }

    bool checkLink(size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            return L_[z - T_.size()];
        } else {
            return T_[z] ? checkLink(n / k_, p % (n / k_), q % (n / k_), R_.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_)) : false;
        }

    }

    /* getSuccessors() */

    void allSuccessorPositionsIterative(std::vector<size_type>& succs, size_type p) {

        if (L_.empty()) return;

        std::queue<SubrowInfo> queue, nextLevelQueue;
        size_type lenT = T_.size();

        if (lenT == 0) {

            size_type offset = p * nPrime_;
            for (size_type i = 0; i < nPrime_; i++) {
                if (L_[offset + i]) {
                    succs.push_back(i);
                }
            }

        } else {

            // successorsPosInit
            size_type n = nPrime_/ k_;
            size_type relP = p;
            for (size_type j = 0, dq = 0, z = k_ * (relP / n); j < k_; j++, dq += n, z++) {
                queue.push(SubrowInfo(dq, z));
            }

            // successorsPos
            relP %= n;
            n /= k_;
            for (; n > 1; relP %= n, n /= k_) {

                while (!queue.empty()) {

                    auto& cur = queue.front();

                    if (T_[cur.z]) {

                        auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n);

                        for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                            nextLevelQueue.push(SubrowInfo(newDq, y));
                        }

                    }

                    queue.pop();

                }

                queue.swap(nextLevelQueue);

            }


            while (!queue.empty()) {

                auto& cur = queue.front();

                if (T_[cur.z]) {

                    auto y = R_.rank(cur.z + 1) * k_ * k_ + k_ * (relP / n) - lenT;

                    for (size_type j = 0, newDq = cur.dq; j < k_; j++, newDq += n, y++) {
                        if (L_[y]) {
                            succs.push_back(newDq);
                        }
                    }

                }

                queue.pop();

            }

        }

    }

    void successorsInit(std::vector<size_type>& succs, size_type p) {

        if (!L_.empty()) {

            size_type y = k_ * (p / (nPrime_ / k_));

            for (size_type j = 0; j < k_; j++) {
                successors(succs, nPrime_ / k_, p % (nPrime_ / k_), (nPrime_ / k_) * j, y + j);
            }

        }

    }

    void successors(std::vector<size_type>& succs, size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                succs.push_back(q);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + k_ * (p / (n / k_));

                for (size_type j = 0; j < k_; j++) {
                    successors(succs, n / k_, p % (n / k_), q + (n / k_) * j, y + j);
                }

            }

        }


    }

    /* getFirstSuccessor() */

    size_type firstSuccessorPositionIterative(size_type p) {

        if (L_.empty()) return nPrime_;

        if (T_.size() == 0) {

            size_type offset = p * nPrime_;
            for (size_type i = 0; i < nPrime_; i++) {
                if (L_[offset + i]) {
                    return i;
                }
            }

        } else {

            std::stack<ExtendedSubrowInfo> stack;
            stack.emplace(nPrime_ / k_, nPrime_ / k_, p % (nPrime_ / k_), 0, k_ * (p / (nPrime_ / k_)), 0);

            while (!stack.empty()) {

                auto& cur = stack.top();

                if (cur.j == k_) {
                    stack.pop();
                } else {

                    if (cur.z >= T_.size()) {

                        if (L_[cur.z - T_.size()]) {
                            return cur.dq;
                        }

                    } else {

                        if (T_[cur.z]) {
                            stack.emplace(cur.nr / k_, cur.nc / k_, cur.p % (cur.nr / k_), cur.dq, R_.rank(cur.z + 1) * k_ * k_ + k_ * (cur.p / (cur.nr / k_)), 0);
                        }

                    }

                    cur.dq += cur.nc;
                    cur.z++;
                    cur.j++;

                }

            }
        }

        return nPrime_;

    }

    size_type firstSuccessorInit(size_type p) {

        size_type pos = nPrime_;

        if (!L_.empty()) {

            size_type y = k_ * (p / (nPrime_ / k_));

            for (size_type j = 0; j < k_ && pos == nPrime_; j++) {
                pos = firstSuccessor(nPrime_ / k_, p % (nPrime_ / k_), (nPrime_ / k_) * j, y + j);
            }

        }

        return pos;

    }

    size_type firstSuccessor(size_type n, size_type p, size_type q, size_type z) {

        size_type pos = nPrime_;

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                pos = q;
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + k_ * (p / (n / k_));

                for (size_type j = 0; j < k_ && pos == nPrime_; j++) {
                    pos = firstSuccessor(n / k_, p % (n / k_), q + (n / k_) * j, y + j);
                }

            }

        }

        return pos;

    }

    /* getPredecessors() */

    void predecessorsInit(std::vector<size_type>& preds, size_type q) {

        if (!L_.empty()) {

            size_type y = q / (nPrime_ / k_);

            for (size_type i = 0; i < k_; i++) {
                predecessors(preds, nPrime_ / k_, q % (nPrime_ / k_), (nPrime_ / k_) * i, y + i * k_);
            }

        }

    }

    void predecessors(std::vector<size_type>& preds, size_type n, size_type q, size_type p, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                preds.push_back(p);
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_ + q / (n / k_);

                for (size_type i = 0; i < k_; i++) {
                    predecessors(preds, n / k_, q % (n / k_), p + (n / k_) * i, y + i * k_);
                }

            }

        }

    }

    /* getRange() */

    void rangeInit(positions_type& pairs, size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            size_type p1Prime, p2Prime;

            for (size_type i = p1 / (nPrime_ / k_); i <= p2 / (nPrime_ / k_); i++) {

                p1Prime = (i == p1 / (nPrime_ / k_)) * (p1 % (nPrime_ / k_));
                p2Prime = (i == p2 / (nPrime_ / k_)) ? p2 % (nPrime_ / k_) : (nPrime_ / k_) - 1;

                for (size_type j = q1 / (nPrime_ / k_); j <= q2 / (nPrime_ / k_); j++) {
                    range(
                            pairs,
                            nPrime_ / k_,
                            p1Prime,
                            p2Prime,
                            (j == q1 / (nPrime_ / k_)) * (q1 % (nPrime_ / k_)),
                            (j == q2 / (nPrime_ / k_)) ? q2 % (nPrime_ / k_) : (nPrime_ / k_) - 1,
                            (nPrime_ / k_) * i,
                            (nPrime_ / k_) * j,
                            k_ * i + j
                    );
                }

            }

        }

    }

    void range(positions_type& pairs, size_type n, size_type p1, size_type p2, size_type q1, size_type q2, size_type dp, size_type dq, size_type z) {

        if (z >= T_.size()) {

            if (L_[z - T_.size()]) {
                pairs.push_back(std::make_pair(dp, dq));
            }

        } else {

            if (T_[z]) {

                size_type y = R_.rank(z + 1) * k_ * k_;
                size_type p1Prime, p2Prime;

                for (size_type i = p1 / (n / k_); i <= p2 / (n / k_); i++) {

                    p1Prime = (i == p1 / (n / k_)) * (p1 % (n / k_));
                    p2Prime = (i == p2 / (n / k_)) ? p2 % (n / k_) : n / k_ - 1;

                    for (size_type j = q1 / (n / k_); j <= q2 / (n / k_); j++) {
                        range(
                                pairs,
                                n / k_,
                                p1Prime,
                                p2Prime,
                                (j == q1 / (n / k_)) * (q1 % (n / k_)),
                                (j == q2 / (n / k_)) ? q2 % (n / k_) : n / k_ - 1,
                                dp + (n / k_) * i,
                                dq + (n / k_) * j,
                                y + k_ * i + j
                        );
                    }

                }

            }

        }

    }

    /* linkInRange() */

    bool linkInRangeInit(size_type p1, size_type p2, size_type q1, size_type q2) {

        if (!L_.empty()) {

            // dividing by k_ (as stated in the paper) in not correct,
            // because it does not use the size of the currently considered submatrix but of its submatrices
            if ((p1 == 0) && (q1 == 0) && (p2 == (nPrime_ /*/ k_*/ - 1)) && (q2 == (nPrime_ /*/ k_*/ - 1))) {
                return 1;
            }

            size_type p1Prime, p2Prime;

            for (size_type i = p1 / (nPrime_ / k_); i <= p2 / (nPrime_ / k_); i++) {

                p1Prime = (i == p1 / (nPrime_ / k_)) * (p1 % (nPrime_ / k_));
                p2Prime = (i == p2 / (nPrime_ / k_)) ? (p2 % (nPrime_ / k_)) : nPrime_ / k_ - 1;

                for (size_type j = q1 / (nPrime_ / k_); j <= q2 / (nPrime_ / k_); j++) {

                    if (linkInRange(nPrime_ / k_, p1Prime, p2Prime, (j == q1 / (nPrime_ / k_)) * (q1 % (nPrime_ / k_)), (j == q2 / (nPrime_ / k_)) ? q2 % (nPrime_ / k_) : nPrime_ / k_ - 1, k_ * i + j)) {
                        return true;
                    }

                }

            }

        }

        return false;

    }

    bool linkInRange(size_type n, size_type p1, size_type p2, size_type q1, size_type q2, size_type z) {

        if (z >= T_.size()) {

            return L_[z - T_.size()];

        } else {

            if (T_[z]) {

                // dividing by k_ (as stated in the paper) in not correct,
                // because it does not use the size of the currently considered submatrix but of its submatrices
                if ((p1 == 0) && (q1 == 0) && (p2 == (n /*/ k_*/ - 1)) && (q2 == (n /*/ k_*/ - 1))) {
                    return true;
                }

                size_type y = R_.rank(z + 1) * k_ * k_;
                size_type p1Prime, p2Prime;

                for (size_type i = p1 / (n / k_); i <= p2 / (n / k_); i++) {

                    p1Prime = (i == p1 / (n / k_)) * (p1 % (n / k_));
                    p2Prime = (i == p2 / (n / k_)) ? (p2 % (n / k_)) : n / k_ - 1;

                    for (size_type j = q1 / (n / k_); j <= q2 / (n / k_); j++) {

                        if (linkInRange(n / k_, p1Prime, p2Prime, (j == q1 / (n / k_)) * (q1 % (n / k_)), (j == q2 / (n / k_)) ? q2 % (n / k_) : n / k_ - 1, y + k_ * i + j)) {
                            return true;
                        }

                    }

                }

            }

            return false;

        }

    }


    /* setNull() */

    void setInit(size_type p, size_type q) {

        if (!L_.empty()) {
            set(nPrime_ / k_, p % (nPrime_ / k_), q % (nPrime_ / k_), (p / (nPrime_ / k_)) * k_ + q / (nPrime_ / k_));
        }

    }

    void set(size_type n, size_type p, size_type q, size_type z) {

        if (z >= T_.size()) {
            L_[z - T_.size()] = null_;
        } else {

            if (T_[z]) {
                set(n / k_, p % (n / k_), q % (n / k_), R_.rank(z + 1) * k_ * k_ + (p / (n / k_)) * k_ + q / (n / k_));
            }

        }

    }

};

#endif //K2TREES_STATICBASICTREE_HPP
