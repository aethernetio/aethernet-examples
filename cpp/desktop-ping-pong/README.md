# Ping Pong for Desktop

This is a simple ping pong example using the *aether* client library. It registers two clients, *Alice* and *Bob*. *Alice* sends "ping", and *Bob* responds with "pong".
This is the simplest example, but it covers many features of the *aether* library: `aether_app`, objects, actions, events, and streams.

## Build
Simply use the provided scripts for different operating systems:
```sh
build_and_run.sh
```
```sh
build_and_run.bat
```

## Deep Dive
### The Recipe to Build
We are using **CMake** for now. Although some consider it an industry standard, other build systems will also be supported in the future.
The *aether* library requires at least *C++17*, so ensure your project requires it and refer to the following code snippet.

```cmake
# By default, distillation mode is off; turn it on for this example
if (NOT DEFINED AE_DISTILLATION)
  set(AE_DISTILLATION On)
endif()

# Add a user-provided config file, which will be included as a regular .h file
set(USER_CONFIG user_config.h CACHE PATH "" FORCE)
# ${USER_CONFIG} must be an absolute path or a path to something listed in include directories
include_directories(${CMAKE_CURRENT_LIST_DIR})

# Add the Aether library dependency
add_subdirectory(aether-client-cpp/aether aether)
```

There are two modes in which *aether* operates: [distillation mode](https://aethernet.io/documentation#c++2) and production mode.
By default, *aether* builds in production mode, but for this example, we set the `AE_DISTILLATION` option to `On` directly in the CMake script.
In distillation mode, *aether* configures all its inner objects to default states and saves them to the file system.
Check the `build/state` directory.
In production mode, *aether* only loads objects with saved states from `./state`.
This not only saves time and code required to configure large objects but, more importantly, allows the use of saved states between application runs.

To configure the *aether* library, we use a configuration header file. All configuration options with their default values are listed in `aether/config.h`.
However, users can provide their own configuration through the `USER_CONFIG` option.
This must be an absolute path or a path relative to something listed in the include directories (compilers `-I` option).

### Where It All Begins
```cpp
int main() {
  auto aether_app = ae::AetherApp::Construct(ae::AetherAppConstructor{});

  std::unique_ptr<Alice> alice;
  std::unique_ptr<Bob> bob;
  TimeSynchronizer time_synchronizer;

  auto client_register_action = ClientRegister{*aether_app};

  // Create a subscription to the Result event
  client_register_action.ResultEvent().Subscribe([&](auto const& action) {
    auto [client_alice, client_bob] = action.get_clients();
    alice = ae::make_unique<Alice>(*aether_app, std::move(client_alice),
                                   time_synchronizer, client_bob->uid());
    bob = ae::make_unique<Bob>(*aether_app, client_bob, time_synchronizer);
    // Save the current aether state
    aether_app->domain().SaveRoot(aether_app->aether());
  });

  // Subscription to the Error event
  client_register_action.ErrorEvent().Subscribe(
      [&](auto const&) { aether_app->Exit(1); });

  while (!aether_app->IsExited()) {
    auto next_time = aether_app->Update(ae::Now());
    aether_app->WaitUntil(next_time);
  }
  return aether_app->ExitCode();
}
```

Let's go through this!

First, create `aether_app` — it's the single object in the *aether* library that rules them all. It creates, initializes, and provides access to the root `aether` object.
It also includes helper functions like `Update` and `WaitUntil` to easily integrate it into your update/event loop.

Define our main characters, *Alice* and *Bob*.

Create an action to register clients in Aethernet — `client_register_action`.
An [action](https://aethernet.io/technology#action2) in *aether* is a concept for performing asynchronous operations.
Each action inherits from `ae::Action<T>` and is registered in the [ActionProcessor](https://aethernet.io/technology#action2) infrastructure.
It has an `Update` method invoked every loop, where we can manage a state machine or check the status of multithreaded tasks.
To inform about its state, three [events](https://aethernet.io/documentation#c++2) exist: `Result`, `Error`, and `Stop` — the names speak for themselves.

We subscribe to the `Result` event and obtain clients for *Alice* and *Bob* from the action to initialize our characters and also save the current `aether` state.
On `Error`, we close the application with exit code 1 as soon as possible.

For this example, clients for `Alice` and `Bob` are registered every time the application runs in distillation mode.
However, in production mode, clients from the saved state are used.
Reconfigure CMake with `AE_DISTILLATION=Off` (just run `cmake -DAE_DISTILLATION=Off .` in your build directory) and rebuild it.
The next run will be slightly faster without registration.

[Event subscription](https://aethernet.io/documentation#c++2) is a RAII object that holds a subscription to events and unsubscribes upon destruction.

### Alice
```cpp
Alice::Alice(ae::AetherApp& aether_app, ae::Client::ptr client_alice,
             TimeSynchronizer& time_synchronizer, ae::Uid bobs_uid)
    : aether_{aether_app.aether()},
      client_alice_{std::move(client_alice)},
      p2pstream_{ae::ActionContext{*aether_->action_processor},
                 kSafeStreamConfig,
                 ae::make_unique<ae::P2pStream>(
                     ae::ActionContext{*aether_->action_processor},
                     client_alice_, bobs_uid)},
      interval_sender_{ae::ActionContext{*aether_->action_processor},
                       time_synchronizer, p2pstream_,
                       std::chrono::milliseconds{5000}},
      interval_sender_subscription_{interval_sender_.ErrorEvent().Subscribe(
          [&](auto const&) { aether_app.Exit(1); })} {}
```

*Alice* saves a pointer to the `aether` object, stores `client_alice`, and creates entities where all the business magic happens.
She knows *Bob*'s [`uid`](https://aethernet.io/technology#registering-new-client0) and doesn't mind chatting with him.

Create `p2pstream_`. It's a [`P2pSafeStream`](https://aethernet.io/documentation#c++2),
where *Safe* means it guarantees or makes every effort to deliver *Alice's* message to *Bob*.
Think of [streams](https://aethernet.io/documentation#c++2) as tunnels through the internet and Aethernet servers to another client.
They are full-duplex, so you can send messages through the tunnel and receive responses simultaneously.

*Alice* uses `IntervalSender` — another action — to send her "pings" periodically.

```cpp
Alice::IntervalSender::IntervalSender(ae::ActionContext action_context,
                                      TimeSynchronizer& time_synchronizer,
                                      ae::ByteIStream& stream,
                                      ae::Duration interval)
    : ae::Action<IntervalSender>{action_context},
      stream_{stream},
      time_synchronizer_{&time_synchronizer},
      interval_{interval},
      response_subscription_{stream.out_data_event().Subscribe(
          *this, ae::MethodPtr<&IntervalSender::ResponseReceived>{})} {}

ae::ActionResult Alice::IntervalSender::Update() {
  auto current_time = ae::Now();
  if (sent_time_ + interval_ <= current_time) {
    constexpr std::string_view ping_message = "ping";

    time_synchronizer_->SetPingSentTime(current_time);

    std::cout << ae::Format("[{:%H:%M:%S}] Alice sends \"ping\"'\n", ae::Now());
    auto send_action =
        stream_.Write({std::begin(ping_message), std::end(ping_message)});

    // notify about error
    send_subscriptions_.Push(
        send_action->ErrorEvent().Subscribe([&](auto const&) {
          std::cerr << "ping send error" << '\n';
          ae::Action<IntervalSender>::Error(*this);
        }));

    sent_time_ = current_time;
  }

  return ae::ActionResult::Delay(sent_time_ + interval_);
}
```

`IntervalSender` waits for `interval_` time from the last `sent_time_` and sends a new "ping" then. It is also subscribed to response messages.

### Bob
```cpp
Bob::Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob,
         TimeSynchronizer& time_synchronizer)
    : aether_{aether_app.aether()},
      client_bob_{std::move(client_bob)},
      time_synchronizer_{&time_synchronizer},
      new_stream_receive_subscription_{
          client_bob_->client_connection()->new_stream_event().Subscribe(
              *this, ae::MethodPtr<&Bob::OnNewStream>{})} {}

void Bob::OnNewStream(ae::Uid destination_uid,
                      std::unique_ptr<ae::ByteIStream> message_stream) {
  p2pstream_ = ae::make_unique<ae::P2pSafeStream>(
      ae::ActionContext{*aether_->action_processor}, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(
          ae::ActionContext{*aether_->action_processor}, client_bob_,
          destination_uid, std::move(message_stream)));
  StreamCreated(*p2pstream_);
}

void Bob::StreamCreated(ae::ByteIStream& stream) {
  message_receive_subscription_ =
      stream.out_data_event().Subscribe([&](auto const& data_buffer) {
        auto ping_message =
            std::string_view{reinterpret_cast<char const*>(data_buffer.data()),
                             data_buffer.size()};
        std::cout << ae::Format(
            "[{:%H:%M:%S}] Bob received \"{}\" within time {} ms\n", ae::Now(),
            ping_message,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                time_synchronizer_->GetPingDuration())
                .count());

        time_synchronizer_->SetPongSentTime(ae::Now());
        constexpr std::string_view pong_message = "pong";
        std::cout << ae::Format("[{:%H:%M:%S}] Bob sends \"pong\"\n",
                                ae::Now());
        stream.Write({std::begin(pong_message), std::end(pong_message)});
      });
}
```

*Bob* forgot to ask for *Alice*'s number (uid) and hopes she might message him first.
He subscribes to the `new_stream_event` of his Aethernet connection.
When *Alice* sends the first message to the stream, Aethernet routes it directly to *Bob* through the server.
*Bob* then creates a `P2pSafeStream` with the same properties as *Alice*'s, allowing him to parse, decrypt, and receive her exact message.
Excited, and not wanting to keep her waiting, *Bob* promptly sends his "pong" in response.

## The End
I couldn't find the strength to stop their chatting. So, once you're tired of them, hit `Ctrl+C` or kill the process.
