add_artery_feature(mrsu_bremen2_test TestBroadcastServiceBremen.cc SegmentCoordinationServer.cc MrsuModuleMapper.cc CpmLogger.cc)
target_link_libraries(mrsu_bremen2_test PRIVATE -Wl,--no-as-needed envmod -Wl,--as-needed)
add_opp_message(
    mrsu-bremen2-test-broadcast-message
    MESSAGE TestBroadcast.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_bremen2_test PRIVATE mrsu-bremen2-test-broadcast-message)

add_opp_message(
    mrsu-bremen2-relay-message
    MESSAGE MrsuRelay.msg
    MSG_PREFIX test_msgs
)
target_link_libraries(mrsu_bremen2_test PRIVATE mrsu-bremen2-relay-message)
add_opp_run(mrsu_bremen2 CONFIG omnetpp.ini NED_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR})