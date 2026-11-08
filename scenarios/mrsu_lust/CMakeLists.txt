add_artery_feature(mrsu_lust_test TestBroadcastServiceBremen.cc SegmentCoordinationServer.cc MrsuModuleMapper.cc CpmLogger.cc TestCpmServiceBremen.cc)
target_link_libraries(mrsu_lust_test PRIVATE -Wl,--no-as-needed envmod -Wl,--as-needed)
add_opp_message(
    mrsu-lust-test-broadcast-message
    MESSAGE TestBroadcast.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_lust_test PRIVATE mrsu-lust-test-broadcast-message)
add_opp_message(
    mrsu-lust-relay-message
    MESSAGE MrsuRelay.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_lust_test PRIVATE mrsu-lust-relay-message)
add_opp_message(
    mrsu-lust-cpm-message
    MESSAGE TestCpm.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_lust_test PRIVATE mrsu-lust-cpm-message)
add_opp_run(mrsu_lust CONFIG omnetpp.ini NED_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR})
