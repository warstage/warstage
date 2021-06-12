// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <boost/test/unit_test.hpp>
#include <runtime/object.h>
#include "runtime/ownership.h"

Property* makePropertyMock(OwnershipState ownershipState) {
    auto result = new Property{nullptr, ""};
    result->masterOwnership_.first = ownershipState;
    return result;
}

OwnershipNotification testOwnershipNotification(const Property* property) {
    return property->masterOwnership_.second;
}

BOOST_AUTO_TEST_SUITE(runtime_update_ownership_notifications)
    
    BOOST_AUTO_TEST_CASE(negotiated_ownership_divestiture_1) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::NegotiatedOwnershipDivestiture);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::RequestOwnershipAssumption, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(negotiated_ownership_divestiture_2) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::WillingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::NegotiatedOwnershipDivestiture);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipDivestitureNotification, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipAcquisitionNotification, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(ownership_acquisition_1) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::OwnershipAcquisition);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::RequestOwnershipRelease, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(ownership_acquisition_2) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::OwnershipAcquisition);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipAcquisitionNotification, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipDivestitureNotification, testOwnershipNotification(ownership[1]));
    }

    BOOST_AUTO_TEST_CASE(ownership_acquisition_if_available_1) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease));

        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::OwnershipAcquisitionIfAvailable);

        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipUnavailable, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[1]));
    }

    BOOST_AUTO_TEST_CASE(ownership_acquisition_if_available_2) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));

        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::Divesting
            + OwnershipStateFlag::NotAskedToRelease));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::OwnershipAcquisitionIfAvailable);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipAcquisitionNotification, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipDivestitureNotification, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(ownership_acquisition_if_available_3) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::OwnershipAcquisitionIfAvailable);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipAcquisitionNotification, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(ownership_release_success_1) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::AskedToRelease));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::OwnershipReleaseSuccess);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::RequestOwnershipAssumption, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(ownership_release_success_2) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::AskedToRelease));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::WillingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::OwnershipReleaseSuccess);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipAcquisitionNotification, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(unconditional_ownership_divestiture_1) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::UnconditionalOwnershipDivestiture);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::RequestOwnershipAssumption, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(unconditional_ownership_divestiture_2) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::AcquisitionPending
            + OwnershipStateFlag::Acquiring
            + OwnershipStateFlag::NotTryingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::UnconditionalOwnershipDivestiture);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipAcquisitionNotification, testOwnershipNotification(ownership[1]));
    }
    
    BOOST_AUTO_TEST_CASE(unconditional_ownership_divestiture_3) {
        std::vector<Property*> ownership{};
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Owned
            + OwnershipStateFlag::NotDivesting
            + OwnershipStateFlag::NotAskedToRelease));
        ownership.push_back(makePropertyMock(OwnershipState{}
            + OwnershipStateFlag::Unowned
            + OwnershipStateFlag::AbleToAcquire
            + OwnershipStateFlag::NotAcquiring
            + OwnershipStateFlag::WillingToAcquire));
        
        updateOwnershipNotifications(ownership, *ownership[0], OwnershipOperation::UnconditionalOwnershipDivestiture);
        
        BOOST_CHECK_EQUAL(OwnershipNotification::None, testOwnershipNotification(ownership[0]));
        BOOST_CHECK_EQUAL(OwnershipNotification::OwnershipAcquisitionNotification, testOwnershipNotification(ownership[1]));
    }

BOOST_AUTO_TEST_SUITE_END()
