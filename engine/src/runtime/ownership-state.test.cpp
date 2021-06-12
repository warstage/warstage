// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include "runtime/ownership.h"

BOOST_AUTO_TEST_SUITE(runtime_update_ownership_state)
    
    BOOST_AUTO_TEST_CASE(publish) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::NotAbleToAcquire,
            OwnershipOperation::Publish);
        bool success = updateOwnershipState(ownership, OwnershipOperation::Publish);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::Publish, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(cancel_negotiated_ownership_divestiture) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::CancelNegotiatedOwnershipDivestiture);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::CancelNegotiatedOwnershipDivestiture, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(cancel_negotiated_ownership_divestiture_undo) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease,
            OwnershipOperation::NegotiatedOwnershipDivestiture);
        bool success = updateOwnershipState(ownership, OwnershipOperation::CancelNegotiatedOwnershipDivestiture);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::None, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(cancel_ownership_acquisition) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::AcquisitionPending
            + OwnershipStateFlag::Acquiring
            + OwnershipStateFlag::NotTryingToAcquire,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::CancelOwnershipAcquisition);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::AcquisitionPending
            + OwnershipStateFlag::TryingToCancelAcquisition
            + OwnershipStateFlag::NotTryingToAcquire, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::CancelOwnershipAcquisition, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(negotiated_ownership_divestiture) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::NegotiatedOwnershipDivestiture);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::NegotiatedOwnershipDivestiture, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(negotiated_ownership_divestiture_undo) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease,
            OwnershipOperation::CancelNegotiatedOwnershipDivestiture);
        bool success = updateOwnershipState(ownership, OwnershipOperation::NegotiatedOwnershipDivestiture);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::None, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(ownership_acquisition) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::WillingToAcquire,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::OwnershipAcquisition);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::AcquisitionPending
            + OwnershipStateFlag::Acquiring
            + OwnershipStateFlag::NotTryingToAcquire, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::OwnershipAcquisition, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(ownership_acquisition_if_available) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::OwnershipAcquisitionIfAvailable);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::WillingToAcquire, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::OwnershipAcquisitionIfAvailable, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(ownership_release_failure) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::AskedToRelease,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::OwnershipReleaseFailure);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::OwnershipReleaseFailure, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(ownership_release_success) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::AskedToRelease,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::OwnershipReleaseSuccess);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::OwnershipReleaseSuccess, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(unconditional_ownership_divestiture) {
        auto ownership = std::make_pair(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease,
            OwnershipOperation::None);
        bool success = updateOwnershipState(ownership, OwnershipOperation::UnconditionalOwnershipDivestiture);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, ownership.first.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire, ownership.first);
        BOOST_CHECK_EQUAL(OwnershipOperation::UnconditionalOwnershipDivestiture, ownership.second);
    }
    
    BOOST_AUTO_TEST_CASE(confirm_ownership_acquisition_cancellation) {
        auto state = OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::AcquisitionPending
            + OwnershipStateFlag::TryingToCancelAcquisition
            + OwnershipStateFlag::NotTryingToAcquire;
        bool success = updateOwnershipState(state, OwnershipNotification::ConfirmOwnershipAcquisitionCancellation);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, state.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire, state);
    }

    BOOST_AUTO_TEST_CASE(ownership_acquisition_notification_1) {
        auto state = OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::AcquisitionPending
            + OwnershipStateFlag::Acquiring
            + OwnershipStateFlag::NotTryingToAcquire;
        bool success = updateOwnershipState(state, OwnershipNotification::OwnershipAcquisitionNotification);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, state.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease, state);
    }
    
    BOOST_AUTO_TEST_CASE(ownership_acquisition_notification_2) {
        auto state = OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::AcquisitionPending
            + OwnershipStateFlag::TryingToCancelAcquisition
            + OwnershipStateFlag::NotTryingToAcquire;
        bool success = updateOwnershipState(state, OwnershipNotification::OwnershipAcquisitionNotification);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, state.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease, state);
    }
    
    BOOST_AUTO_TEST_CASE(ownership_acquisition_notification_3) {
        auto state = OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::WillingToAcquire;
        bool success = updateOwnershipState(state, OwnershipNotification::OwnershipAcquisitionNotification);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, state.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease, state);
    }
    
    BOOST_AUTO_TEST_CASE(ownership_divestiture_notification) {
        auto state = OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease;
        bool success = updateOwnershipState(state, OwnershipNotification::OwnershipDivestitureNotification);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, state.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire, state);
    }

    BOOST_AUTO_TEST_CASE(ownership_unavailable) {
        auto state = OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::WillingToAcquire;
        bool success = updateOwnershipState(state, OwnershipNotification::OwnershipUnavailable);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, state.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire, state);
    }
    
    BOOST_AUTO_TEST_CASE(request_ownership_release) {
        auto state = OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease;
        bool success = updateOwnershipState(state, OwnershipNotification::RequestOwnershipRelease);
        BOOST_CHECK_EQUAL(true, success);
        BOOST_CHECK_EQUAL(true, state.validate());
        BOOST_CHECK_EQUAL(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::AskedToRelease, state);
    }

BOOST_AUTO_TEST_SUITE_END()
