# Copyright Felix Ungman. All rights reserved.
# Licensed under GNU General Public License version 3 or later.

cmake_minimum_required(VERSION 3.13)

project(warstage-engine)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS true)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_SWIZZLE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT=1")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(SYSTEM
        /usr/include/c++/10
        lib/boost-1.76.0
        lib/glm-0.9.9.8
        lib/khronos
        )

include_directories(
        src
        )

link_libraries(
        Threads::Threads
)

add_executable(warstage-engine
        main.cpp
        src/async/mutex.cpp
        src/async/mutex.test.cpp
        src/async/promise.cpp
        src/async/promise.test.cpp
        src/async/shutdownable.cpp
        src/async/shutdownable.test.cpp
        src/async/strand-asio.cpp
        src/async/strand-base.cpp
        src/async/strand-manual.cpp
        src/async/strand.cpp
        src/async/strand.test.cpp
        src/battle-audio/sound-director.cpp
        src/battle-gestures/camera-control.cpp
        src/battle-gestures/camera-gesture.cpp
        src/battle-gestures/command-gesture.cpp
        src/battle-gestures/editor-gesture.cpp
        src/battle-gestures/editor-model.cpp
        src/battle-gestures/unit-controller.cpp
        src/battle-model/battle-vm.cpp
        src/battle-model/height-map.cpp
        src/battle-model/height-map.test.cpp
        src/battle-model/image-tiles.cpp
        src/battle-model/terrain-map.cpp
        src/battle-simulator/battle-objects.cpp
        src/battle-simulator/battle-simulator.cpp
        src/battle-simulator/convert-value.cpp
        src/battle-view/battle-animator.cpp
        src/battle-view/battle-view.cpp
        src/battle-view/camera-state.cpp
        src/battle-view/render-background.cpp
        src/battle-view/render-body.cpp
        src/battle-view/render-debug.cpp
        src/battle-view/render-facing.cpp
        src/battle-view/render-marker.cpp
        src/battle-view/render-movement.cpp
        src/battle-view/render-phantom.cpp
        src/battle-view/render-range.cpp
        src/battle-view/render-selection.cpp
        src/battle-view/render-sky.cpp
        src/battle-view/render-terrain.cpp
        src/battle-view/render-water.cpp
        src/battle-view/shaders.cpp
        src/geometry/b-spline-patch.cpp
        src/geometry/b-spline.cpp
        src/geometry/geometry.cpp
        src/geometry/quad-tree.cpp
        src/geometry/velocity-sampler.cpp
        src/gesture/gesture.cpp
        src/gesture/pointer.cpp
        src/gesture/surface.cpp
        src/graphics/framebuffer.cpp
        src/graphics/graphics-api.cpp
        src/graphics/graphics.cpp
        src/graphics/pipeline.cpp
        src/graphics/program.cpp
        src/graphics/renderbuffer.cpp
        src/graphics/sampler.cpp
        src/graphics/shader.cpp
        src/graphics/texture.cpp
        src/graphics/vertex-buffer.cpp
        src/graphics/vertex.cpp
        src/graphics/viewport.cpp
        src/image/gaussian-blur.cpp
        src/image/image.cpp
        src/image/lodepng.cpp
        src/matchmaker/lobby-supervisor.cpp
        src/matchmaker/lobby-supervisor.test.cpp
        src/matchmaker/master-supervision-policy.cpp
        src/matchmaker/battle-supervisor.cpp
        src/player/player-session.cpp
        src/player/player-endpoint.cpp
        src/player/player-frontend.cpp
        src/player/player-backend.cpp
        src/player/player-window.cpp
        src/runtime/web-socket-endpoint.cpp
        src/runtime/mock-endpoint.cpp
        src/runtime/endpoint.cpp
        src/runtime/event-class.cpp
        src/runtime/federate.cpp
        src/runtime/federation.cpp
        src/runtime/object-class.cpp
        src/runtime/object.cpp
        src/runtime/ownership-state.test.cpp
        src/runtime/ownership.cpp
        src/runtime/ownership.test.cpp
        src/runtime/runtime-fixture-auto_correct.test.cpp
        src/runtime/runtime-fixture-ownership_divestiture.test.cpp
        src/runtime/runtime-fixture-ownership_negotiation.test.cpp
        src/runtime/runtime-fixture-ownership_policy.test.cpp
        src/runtime/runtime-fixture-ownership_unpublish.test.cpp
        src/runtime/runtime-fixture-startup-shutdown.test.cpp
        src/runtime/runtime-fixture-sync_object.test.cpp
        src/runtime/runtime.cpp
        src/runtime/service-class.cpp
        src/runtime/web-socket-session.cpp
        src/runtime/session-federate.cpp
        src/runtime/mock-session.cpp
        src/runtime/session.cpp
        src/runtime/supervision-policy.cpp
        src/utilities/logging.cpp
        src/utilities/memory.test.cpp
        src/value/builder.test.cpp
        src/value/compressor.cpp
        src/value/compressor.test.cpp
        src/value/decompressor.cpp
        src/value/dictionary.cpp
        src/value/json.cpp
        src/value/json.test.cpp
        src/value/value.cpp
        src/value/value.test.cpp
        )

add_test(warstage-engine warstage-engine
        --logger=HRF,all
        --color_output=false
        --report_format=HRF
        --show_progress=no
        )