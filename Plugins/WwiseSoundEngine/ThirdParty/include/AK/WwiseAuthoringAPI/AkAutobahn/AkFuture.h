/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#pragma once

#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#include <optional>
#include <memory>

namespace AK
{
	template<typename T>
	class AkPromise;

	/// Internal namespace for the shared state implementation details.
	namespace Internal
	{
		/// The shared state that connects a promise and a future.
		template<typename T>
		struct AkSharedState
		{
			CAkLock m_lock;
			AkEvent m_event;
			std::optional<T> m_value;
			bool m_bPromiseBroken = false;

			AkSharedState() { AKPLATFORM::AkCreateEvent(m_event); }
			~AkSharedState() { AKPLATFORM::AkDestroyEvent(m_event); }

			// The shared state is non-copyable.
			AkSharedState(const AkSharedState&) = delete;
			AkSharedState& operator=(const AkSharedState&) = delete;
		};
	} // namespace Internal

	/// Represents a future result of an asynchronous operation.
	template<typename T>
	class AkFuture
	{
	public:
		AkFuture() = default;

		// Futures are move-only entities.
		AkFuture(const AkFuture&) = delete;
		AkFuture& operator=(const AkFuture&) = delete;
		AkFuture(AkFuture&& other) noexcept = default;
		AkFuture& operator=(AkFuture&& other) noexcept = default;

		/// Checks if the future holds a valid shared state.
		bool valid() const { return m_pSharedState != nullptr; }

		/// Blocks until the result is available.
		/// @param out_value Reference to store the retrieved value.
		/// @return true if the value was retrieved successfully.
		bool get(T& out_value)
		{
			if (!valid()) return false;

			wait();

			{
				AkAutoLock<CAkLock> lock(m_pSharedState->m_lock);

				if (m_pSharedState->m_bPromiseBroken || !m_pSharedState->m_value.has_value())
					return false;

				out_value = std::move(*(m_pSharedState->m_value));
			}

			// Invalidate this future, as get() can only be called once.
			m_pSharedState.reset();

			return true;
		}

		/// Blocks until the associated promise is fulfilled or broken.
		void wait() const
		{
			if (!valid()) return;

			// First, check the state under a lock. If ready, no need to wait on the kernel event.
			{
				AkAutoLock<CAkLock> lock(m_pSharedState->m_lock);

				if (m_pSharedState->m_value.has_value() || m_pSharedState->m_bPromiseBroken)
					return;
			}

			// Wait for the promise to signal the event.
			AKPLATFORM::AkWaitForEvent(m_pSharedState->m_event);

			// The provided event is auto-reset. To allow multiple waiters (e.g., multiple
			// calls to wait()), we must re-signal the event after we consume it.
			AKPLATFORM::AkSignalEvent(m_pSharedState->m_event);
		}

		/// Waits for the result to become available for a specified duration.
		/// @param in_uWaitMs The timeout duration.
		/// @return true if the result is available, false otherwise.
		bool wait_for(AkUInt32 in_uWaitMs) const
		{
			if (!valid()) return false;

			// First, check the state under a lock. If ready, no need to wait on the kernel event.
			{
				AkAutoLock<CAkLock> lock(m_pSharedState->m_lock);

				if (m_pSharedState->m_value.has_value() || m_pSharedState->m_bPromiseBroken)
					return true;
			}

			bool status = AKPLATFORM::AkWaitForEvent(m_pSharedState->m_event, in_uWaitMs);
			if (status == true)
			{
				// Re-signal for other waiters, consistent with wait()
				AKPLATFORM::AkSignalEvent(m_pSharedState->m_event);
			}
			return status;
		}

	private:
		friend class AkPromise<T>;
		explicit AkFuture(std::shared_ptr<Internal::AkSharedState<T>> state) : m_pSharedState(std::move(state)) {}

		std::shared_ptr<Internal::AkSharedState<T>> m_pSharedState;
	};

	// --- AkPromise ---

	/// Allows setting a value that can be retrieved asynchronously via an AkFuture.
	template<typename T>
	class AkPromise
	{
	public:
		AkPromise() : m_pSharedState(std::make_shared<Internal::AkSharedState<T>>()) {}

		// Promises are move-only entities.
		AkPromise(const AkPromise&) = delete;
		AkPromise& operator=(const AkPromise&) = delete;
		AkPromise(AkPromise&& other) noexcept = default;
		AkPromise& operator=(AkPromise&& other) noexcept = default;

		/// Destructor that handles a "broken promise" situation.
		~AkPromise()
		{
			if (m_pSharedState)
			{
				AkAutoLock<CAkLock> lock(m_pSharedState->m_lock);
				// If the promise is destroyed before a value is set, it is "broken".
				if (!m_pSharedState->m_value.has_value())
				{
					m_pSharedState->m_bPromiseBroken = true;
					AKPLATFORM::AkSignalEvent(m_pSharedState->m_event); // Wake up the future
				}
			}
		}

		/// Retrieves the one and only AkFuture associated with this promise.
		AkFuture<T> get_future()
		{
			// A future can only be retrieved once.
			AKASSERT(m_pSharedState && !m_bFutureRetrieved);
			if (!m_pSharedState || m_bFutureRetrieved)
			{
				return AkFuture<T>(); // Return invalid future
			}
			m_bFutureRetrieved = true;
			return AkFuture<T>(m_pSharedState);
		}

		/// Sets the value of the shared state (copy).
		void set_value(const T& value)
		{
			AKASSERT(m_pSharedState);
			if (!m_pSharedState) return;

			{
				AkAutoLock<CAkLock> lock(m_pSharedState->m_lock);

				if (m_pSharedState->m_value.has_value() || m_pSharedState->m_bPromiseBroken)
					return;

				m_pSharedState->m_value = value;
			}
			
			AKPLATFORM::AkSignalEvent(m_pSharedState->m_event);
		}

		/// Sets the value of the shared state (move).
		void set_value(T&& value)
		{
			AKASSERT(m_pSharedState);
			if (!m_pSharedState) return;

			{
				AkAutoLock<CAkLock> lock(m_pSharedState->m_lock);

				AKASSERT(!m_pSharedState->m_value.has_value() && !m_pSharedState->m_bPromiseBroken);
				if (m_pSharedState->m_value.has_value() || m_pSharedState->m_bPromiseBroken)
					return;

				m_pSharedState->m_value = std::move(value);
			}
			
			AKPLATFORM::AkSignalEvent(m_pSharedState->m_event);
		}

	private:
		std::shared_ptr<Internal::AkSharedState<T>> m_pSharedState;
		bool m_bFutureRetrieved = false;
	};

} // namespace Ak
