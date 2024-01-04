
add_rules("mode.debug", "mode.release")

set_languages("cxx20")

andren_include_dir = "./third_part/submodule/andren/include/"
andren_base_include_dir = andren_include_dir .. "base/"
andren_extention_include_dir = andren_include_dir .. "extention/"

main_include_dir = "./include/"
main_include_boost_tcp_udp_dir = main_include_dir .. "boost_tcp_udp/"
main_include_boost_http_dir = main_include_dir .. "boost_http/"
main_include_boost_websocket_dir = main_include_dir .. "boost_websocket/"
main_include_virtual_session_dir = main_include_dir .. "virtual_session/"

include_dir_flags = " -I " .. andren_base_include_dir
                    .. " -I " .. andren_extention_include_dir
                    .. " -I " .. main_include_dir
                    .. " -I " .. main_include_boost_tcp_udp_dir
                    .. " -I " .. main_include_boost_http_dir 
                    .. " -I " .. main_include_boost_websocket_dir
                    .. " -I " .. main_include_virtual_session_dir


add_cflags("-Wall " .. include_dir_flags )
add_cxxflags("-Wall " .. include_dir_flags )


src_dir = "./src/"
head_dir = "./include/"

src_c_files = src_dir .. "**.c"
src_cc_files = src_dir .. "**.cc"
src_cpp_files = src_dir .. "**.cpp"

head_h_files = head_dir .. "**.h"
head_hh_files = head_dir .. "**.hh"
head_hpp_files = head_dir .. "**.hpp"

andren_head_hpp_file = andren_base_include_dir .. "**.hpp"
andren_head_hh_file = andren_base_include_dir .. "**.hh"
andren_head_h_file = andren_base_include_dir .. "**.h"

includes("third_part/submodule/andren/xmake.lua")

target("andren-boost_a")
    set_kind("static")
    add_headerfiles(head_h_files)
    add_headerfiles(head_hh_files)
    add_headerfiles(head_hpp_files)
    add_headerfiles(andren_head_hpp_file)
    add_headerfiles(andren_head_hh_file)
    add_headerfiles(andren_head_h_file)
    add_files(src_c_files)
    add_files(src_cc_files)
    add_files(src_cpp_files)
    add_deps("andren_a")
    add_links("ssl")
    add_links("uuid")
    add_links("boost_context")
    add_links("boost_coroutine")

target("andren-boost")
    set_kind("shared")
    add_headerfiles(head_h_files)
    add_headerfiles(head_hh_files)
    add_headerfiles(head_hpp_files)
    add_headerfiles(andren_head_hpp_file)
    add_headerfiles(andren_head_hh_file)
    add_headerfiles(andren_head_h_file)
    add_files(src_c_files)
    add_files(src_cc_files)
    add_files(src_cpp_files)
    add_deps("andren")
    add_links("ssl")
    add_links("uuid")
    add_links("boost_context")
    add_links("boost_coroutine")



-------------------
--- examples
--- 
-------------------

example_src_dir = "./example/"

example_test_hello_world_src = example_src_dir .. "test_hello_world.cpp"
example_test_io_context_manager_src = example_src_dir .. "test_io_context_manager.cpp"
example_test_tcp_server_src = example_src_dir .. "test_tcp_server.cpp"
example_test_tcp_client_src = example_src_dir .. "test_tcp_client.cpp"
example_test_udp_src = example_src_dir .. "test_udp.cpp"

example_test_websocket_client_coro_src = example_src_dir .. "test_websocket_client_coro.cpp"
example_test_websocket_server_coro_src = example_src_dir .. "test_websocket_server_coro.cpp"
example_test_websocket_office_awaitable_client_src = example_src_dir .. "test_websocket_office_awaitable_client.cpp"
example_test_websocket_office_awaitable_server_src = example_src_dir .. "test_websocket_office_awaitable_server.cpp"

example_test_andren_websocket_client_src = example_src_dir .. "test_andren_websocket_client.cpp"


target("test_hello_world")
    set_kind("binary")
    add_files(example_test_hello_world_src)
    add_deps("andren-boost_a")

target("test_io_context_manager")
    set_kind("binary")
    add_files(example_test_io_context_manager_src)
    add_deps("andren-boost_a")
    add_links("boost_context")

target("test_tcp_server")
    set_kind("binary")
    add_files(example_test_tcp_server_src)
    add_deps("andren-boost_a")
    add_links("boost_context")

target("test_tcp_client")
    set_kind("binary")
    add_files(example_test_tcp_client_src)
    add_deps("andren-boost_a")
    add_links("boost_context")

target("test_udp")
    set_kind("binary")
    add_files(example_test_udp_src)
    add_deps("andren-boost_a")
    add_links("boost_context")


target("test_websocket_client_coro")
    set_kind("binary")
    add_files(example_test_websocket_client_coro_src)
    add_links("pthread")
    add_deps("andren-boost_a")
    add_links("boost_context")

    
target("test_websocket_server_coro")
    set_kind("binary")
    add_files(example_test_websocket_server_coro_src)
    add_links("pthread")
    add_deps("andren-boost_a")
    add_links("boost_context")

target("test_websocket_office_awaitable_client")
    set_kind("binary")
    add_files(example_test_websocket_office_awaitable_client_src)
    add_links("pthread")
    add_deps("andren-boost_a")
    add_links("boost_context")

target("test_websocket_office_awaitable_server")
    set_kind("binary")
    add_files(example_test_websocket_office_awaitable_server_src)
    add_links("pthread")
    add_deps("andren-boost_a")
    add_links("boost_context")
    

target("test_andren_websocket_client")
    set_kind("binary")
    add_files(example_test_andren_websocket_client_src)
    add_links("pthread")
    add_deps("andren-boost_a")
    add_links("boost_context")

    