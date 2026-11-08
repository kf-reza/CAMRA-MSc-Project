add_artery_feature(mrsu_bremen3_test TestBroadcastServiceBremen.cc SegmentCoordinationServer.cc MrsuModuleMapper.cc CpmLogger.cc TestCpmServiceBremen.cc)
target_link_libraries(mrsu_bremen3_test PRIVATE -Wl,--no-as-needed envmod -Wl,--as-needed)
add_opp_message(
    mrsu-bremen3-test-broadcast-message
    MESSAGE TestBroadcast.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_bremen3_test PRIVATE mrsu-bremen3-test-broadcast-message)
add_opp_message(
    mrsu-bremen3-relay-message
    MESSAGE MrsuRelay.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_bremen3_test PRIVATE mrsu-bremen3-relay-message)
add_opp_message(
    mrsu-bremen3-cpm-message
    MESSAGE TestCpm.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_bremen3_test PRIVATE mrsu-bremen3-cpm-message)
add_opp_run(mrsu_bremen3 CONFIG omnetpp.ini NED_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR})