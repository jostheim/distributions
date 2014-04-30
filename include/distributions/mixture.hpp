// Copyright (c) 2014, Salesforce.com, Inc.  All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// - Neither the name of Salesforce.com nor the names of its contributors
//   may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <vector>
#include <unordered_set>
#include <distributions/common.hpp>
#include <distributions/vector.hpp>
#include <distributions/trivial_hash.hpp>
#include <distributions/random_fwd.hpp>

namespace distributions
{

//----------------------------------------------------------------------------
// Mixture Driver
//
// This interface maintains contiguous groupids for vectorized scoring
// while maintaining a fixed number of empty groups.
// Specific models may use this class, or maintain custom cached scores. 

template<class ModelT, class count_t>
class MixtureDriver
{
public:

    typedef ModelT Model;
    typedef std::unordered_set<size_t, TrivialHash<size_t>> IdSet;

    const std::vector<count_t> & counts () const { return counts_; }
    count_t counts (size_t groupid) const { return counts_[groupid]; }
    const IdSet & empty_groupids () const { return empty_groupids_; }
    size_t sample_size () const { return sample_size_; }

    void init (const Model & model, const std::vector<count_t> & counts)
    {
        counts_ = counts;
        empty_groupids_.clear();
        sample_size_ = 0;

        const size_t group_count = counts_.size();
        for (size_t i = 0; i < group_count; ++i) {
            sample_size_ += counts_[i];
            if (counts_[i] == 0) {
                empty_groupids_.insert(i);
            }
        }
        _validate();
    }

    bool add_value (
            const Model & model,
            size_t groupid,
            count_t count = 1)
    {
        DIST_ASSERT1(count, "cannot add zero values");
        DIST_ASSERT2(groupid < counts_.size(), "bad groupid: " << groupid);

        const bool add_group = (counts_[groupid] == 0);
        counts_[groupid] += count;
        sample_size_ += count;

        if (DIST_UNLIKELY(add_group)) {
            empty_groupids_.erase(groupid);
            empty_groupids_.insert(counts_.size());
            counts_.push_back(0);
            _validate();
        }

        return add_group;
    }

    bool remove_value (
            const Model & model,
            size_t groupid,
            count_t count = 1)
    {
        DIST_ASSERT1(count, "cannot remove zero values");
        DIST_ASSERT2(groupid < counts_.size(), "bad groupid: " << groupid);
        DIST_ASSERT2(counts_[groupid], "cannot remove value from empty group");
        DIST_ASSERT2(count <= counts_[groupid],
            "cannot remove more values than are in group");

        counts_[groupid] -= count;
        sample_size_ -= count;
        const bool remove_group = (counts_[groupid] == 0);

        if (DIST_UNLIKELY(remove_group)) {
            const size_t group_count = counts_.size() - 1;
            if (groupid != group_count) {
                counts_[groupid] = counts_.back();
                if (counts_.back() == 0) {
                    empty_groupids_.erase(group_count);
                    empty_groupids_.insert(groupid);
                }
            }
            counts_.pop_back();
            _validate();
        }

        return remove_group;
    }

    // this slow uncached version should be overridden
    void score_value (const Model & model, AlignedFloats scores) const
    {
        if (DIST_DEBUG_LEVEL >= 1) {
            DIST_ASSERT_EQ(scores.size(), counts_.size());
        }

        const count_t group_count = counts_.size();
        const count_t empty_group_count = empty_groupids_.size();
        const count_t nonempty_group_count = group_count - empty_group_count;
        for (size_t i = 0; i < group_count; ++i) {
            scores[i] = model.score_add_value(
                counts_[i],
                nonempty_group_count,
                sample_size_,
                empty_group_count);
        }
    }

    float score_mixture (const Model & model) const
    {
        return model.score_counts(counts_);
    }

private:

    std::vector<count_t> counts_;
    IdSet empty_groupids_;
    count_t sample_size_;

    void _validate () const
    {
        DIST_ASSERT1(empty_groupids_.size(), "missing empty groups");
        if (DIST_DEBUG_LEVEL >= 2) {
            for (size_t i = 0; i < counts_.size(); ++i) {
                bool count_is_zero = (counts_[i] == 0);
                bool is_empty =
                    (empty_groupids_.find(i) != empty_groupids_.end());
                DIST_ASSERT_EQ(count_is_zero, is_empty);
            }
        }
    }
};


//----------------------------------------------------------------------------
// Mixture Slave

template<class Model>
struct MixtureSlave
{
    typedef typename Model::Group Group;
    typedef typename Model::Value Value;

    std::vector<Group> & groups () { return groups_; }
    Group & groups (size_t groupid)
    {
        DIST_ASSERT1(groupid < groups_.size(), "bad groupid: " << groupid);
        return groups_[groupid];
    }

    const std::vector<Group> & groups () const { return groups_; }
    const Group & groups (size_t groupid) const
    {
        DIST_ASSERT1(groupid < groups_.size(), "bad groupid: " << groupid);
        return groups_[groupid];
    }

    // add_group is called whenever driver.add_value returns true
    void add_group (
            const Model & model,
            rng_t & rng)
    {
        groups_.packed_add().init(model, rng);
    }

    // remove_group is called whenever driver.remove_value returns true
    void remove_group (size_t groupid)
    {
        groups_.packed_remove(groupid);
    }

    void add_value (
            const Model & model,
            size_t groupid,
            const Value & value,
            rng_t & rng)
    {
        groups(groupid).add_value(model, value, rng);
    }

    void remove_value (
            const Model & model,
            size_t groupid,
            const Value & value,
            rng_t & rng)
    {
        groups(groupid).remove_value(model, value, rng);
    }

    // this slow uncached version should be overridden
    void score_value (
            const Model & model,
            const Value & value,
            AlignedFloats scores_accum,
            rng_t & rng) const
    {
        if (DIST_DEBUG_LEVEL >= 2) {
            DIST_ASSERT_EQ(scores_accum.size(), groups_.size());
        }

        const size_t group_count = groups_.size();
        for (size_t i = 0; i < group_count; ++i) {
            scores_accum[i] += groups(i).score(model, value, rng);
        }
    }

    // this slow version should be overridden
    float score_mixture (const Model & model, rng_t & rng) const
    {
        float score = 0;
        for (const Group & group : groups_) {
            score += model.score_group(group, rng);
        }
        return score;
    }

private:

    Packed_<Group> groups_;
};


//----------------------------------------------------------------------------
// Mixture Id Tracker
//
// This interface tracks a mapping between contiguous "packed" group ids
// and fixed unique "global" ids.  Packed ids can change when groups are
// added or removed, but global ids never change.

class MixtureIdTracker
{
public:

    typedef uint32_t Id;

    void init (size_t group_count = 0)
    {
        packed_to_global_.clear();
        global_to_packed_.clear();
        for (size_t i = 0; i < group_count; ++i) {
            add_group();
        }
    }

    void add_group ()
    {
        const Id packed = packed_to_global_.size();
        const Id global = global_to_packed_.size();
        packed_to_global_.push_back(global);
        global_to_packed_.push_back(packed);
    }

    void remove_group (Id packed)
    {
        if (DIST_DEBUG_LEVEL) {
            DIST_ASSERT(packed < packed_size(), "bad packed id: " << packed);
            const Id global = packed_to_global_[packed];
            DIST_ASSERT(global < global_size(), "bad global id: " << global);
            global_to_packed_[global] = ~Id(0);
        }
        const size_t group_count = packed_size() - 1;
        if (packed != group_count) {
            const Id global = packed_to_global_.back();
            DIST_ASSERT1(global < global_size(), "bad global id: " << global);
            packed_to_global_[packed] = global;
            global_to_packed_[global] = packed;
        }
        packed_to_global_.resize(group_count);;
    }

    Id packed_to_global (Id packed) const
    {
        DIST_ASSERT1(packed < packed_size(), "bad packed id: " << packed);
        Id global = packed_to_global_[packed];
        DIST_ASSERT1(global < global_size(), "bad global id: " << global);
        return global;
    }

    Id global_to_packed (Id global) const
    {
        DIST_ASSERT1(global < global_size(), "bad global id: " << global);
        Id packed = global_to_packed_[global];
        DIST_ASSERT1(packed < packed_size(), "bad packed id: " << packed);
        return packed;
    }

    size_t packed_size () const { return packed_to_global_.size(); }
    size_t global_size () const { return global_to_packed_.size(); }

private:

    std::vector<Id> packed_to_global_;
    std::vector<Id> global_to_packed_;
};

} // namespace distributions
