#pragma once
#ifndef PLATFORM_DELEGATES_DELEGATE
#define PLATFORM_DELEGATES_DELEGATE

// Based on https://stackoverflow.com/a/23974539/710069 and https://stackoverflow.com/a/35920804/710069

#include <algorithm>
#include <iostream>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>

namespace Platform::Delegates
{
    template <typename Signature>
    class Delegate;

    template <typename... Args>
    class Delegate<void(Args...)>
    {
        std::vector<std::function<void(Args...)>> callbacks;
        std::mutex mutex;

        Delegate(Delegate const&) = delete;

        void operator=(Delegate const&) = delete;

        static void* GetFunctionTarget(std::function<void(Args...)>& function)
        {
            typedef void(functionType)(Args...);
            functionType** functionPointer = function.template target<functionType*>();
            if (functionPointer == NULL)
            {
                return NULL;
            }
            return *functionPointer;
        }

        static bool AreFunctionsEqual(std::function<void(Args...)>& left, std::function<void(Args...)>& right)
        {
            auto leftTargetPointer = GetFunctionTarget(left);
            auto rightTargetPointer = GetFunctionTarget(right);
            // Only in the case we have to std::functions created using std::bind we have to use alternative way to compare functions
            if (leftTargetPointer == NULL && rightTargetPointer == NULL)
            {
                return AreBoundFounctionsEqual(left, right);
            }
            return leftTargetPointer == rightTargetPointer;
        }

        static bool AreBoundFounctionsEqual(std::function<void(Args...)>& left, std::function<void(Args...)>& right)
        {
            const int size = sizeof(std::function<void(Args...)>);
            std::byte leftArray[size] = { {(std::byte)0} };
            std::byte rightArray[size] = { {(std::byte)0} };
            new (&leftArray) std::function<void(Args...)>(left);
            new (&rightArray) std::function<void(Args...)>(right);

            // PrintBytes(leftArray, rightArray, size);

            // Here the HACK starts
            // By resetting certain values we are able to compare functions correctly
            // When values are reset it has the same effect as when these values are ignored
            ResetAt(leftArray, rightArray, 16);
            ResetAt(leftArray, rightArray, 56);
            ResetAt(leftArray, rightArray, 57);
            // Here the HACK ends

            std::byte* leftByte = leftArray;
            std::byte* rightByte = rightArray;
            for (int i = 0; i < size; i++, leftByte++, rightByte++)
            {
                if (*leftByte != *rightByte)
                {
                    return false;
                }
            }
            return true;
        }

        static void ResetAt(std::byte* leftArray, std::byte* rightArray, size_t i)
        {
            leftArray[i] = (std::byte)0;
            rightArray[i] = (std::byte)0;
        }

        static void PrintBytes(std::byte* leftFirstByte, std::byte* rightFirstByte, unsigned long long size)
        {
            std::cout << "Left: " << std::endl;
            PrintBytes(leftFirstByte, size);
            std::cout << "Right: " << std::endl;
            PrintBytes(rightFirstByte, size);
        }

        static void PrintBytes(std::byte* firstByte, unsigned long long size)
        {
            std::vector<std::byte> vector(firstByte, firstByte + size);
            for (size_t i = 0; i < size; i++)
            {
                std::cout << i << ':' << (int)vector[i] << std::endl;
            }
        }

    public:
        Delegate() {}

        void operator+= (std::function<void(Args...)>&& callback)
        {
            const std::lock_guard<std::mutex> lock(mutex);
            this->callbacks.emplace_back(callback);
        }

        void operator-= (std::function<void(Args...)>&& callback)
        {
            const std::lock_guard<std::mutex> lock(mutex);
            auto deletedRange = std::remove_if(this->callbacks.begin(), this->callbacks.end(),
                [&](std::function<void(Args...)>& other)
                {
                    return AreFunctionsEqual(callback, other);
                });
            this->callbacks.erase(deletedRange, this->callbacks.end());
        }

        void operator()(Args... args)
        {
            const std::lock_guard<std::mutex> lock(mutex);
            for (auto callback : this->callbacks)
            {
                callback(args...);
            }
        }
    };
}

#endif