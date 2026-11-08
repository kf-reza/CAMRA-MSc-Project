add_artery_feature(mrsu_fullcity_test TestBroadcastServiceBremen.cc SegmentCoordinationServer.cc MrsuModuleMapper.cc CpmLogger.cc TestCpmServiceBremen.cc)
target_link_libraries(mrsu_fullcity_test PRIVATE -Wl,--no-as-needed envmod -Wl,--as-needed)
add_opp_message(
    mrsu-fullcity-test-broadcast-message
    MESSAGE TestBroadcast.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_fullcity_test PRIVATE mrsu-fullcity-test-broadcast-message)
add_opp_message(
    mrsu-fullcity-relay-message
    MESSAGE MrsuRelay.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_fullcity_test PRIVATE mrsu-fullcity-relay-message)
add_opp_message(
    mrsu-fullcity-cpm-message
    MESSAGE TestCpm.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_fullcity_test PRIVATE mrsu-fullcity-cpm-message)
add_opp_run(mrsu_fullcity CONFIG omnetpp.ini NED_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR})