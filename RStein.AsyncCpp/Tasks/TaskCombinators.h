#pragma once
#include "Task.h"
#include "TaskCompletionSource.h"
#include "../AsyncPrimitives/AggregateException.h"
#include "../AsyncPrimitives/OperationCanceledException.h"

namespace RStein::AsyncCpp::Tasks
{
  namespace Detail
  {
    template <typename TTask>
    void waitForTask(const TTask& task, std::vector<std::exception_ptr>& exceptions)
    {
      try
      {
        task.Wait();
      }
      catch (...)
      {
        exceptions.push_back(std::current_exception());
      }
    }

    template <typename TTask>
    Task<void> awaitTask(std::vector<std::exception_ptr>& exceptions, const TTask& task)
    {
      try
      {
        co_await task;
      }
      catch (...) 
      {
        exceptions.push_back(std::current_exception());
      }
    }

    template <typename TTaskFirst, typename TTaskSecond, typename... TTaskRest>
    Task<void> awaitTask(std::vector<std::exception_ptr>& exceptions,
                          const TTaskFirst& task,
                          const TTaskSecond& task2,
                          TTaskRest&&... tasksRest)
    {
      co_await awaitTask(exceptions, task);
      co_await awaitTask(exceptions, task2, std::forward<TTaskRest>(tasksRest)...);
    }

    template <typename TTask>
    void waitAnyTask(TTask&& task, TaskCompletionSource<int>& tcs, int taskIndex) 
    {
      auto continuation = [tcs, taskIndex](auto& previous) mutable {tcs.TrySetResult(taskIndex);};
      task.ContinueWith(continuation);      
    }

  }
     
  template <typename... TTask>
  void WaitAll(const TTask&... tasks)
  {
    std::vector<std::exception_ptr> exceptions;
    (Detail::waitForTask(tasks, exceptions), ...);
    if (!exceptions.empty())
    {
      throw AggregateException{exceptions};
    }
  }

  template <typename... TTask>
  Task<void> WhenAll(TTask&&... tasks)
  {
    std::vector<std::exception_ptr> exceptions;

    co_await Detail::awaitTask(exceptions, std::forward<TTask>(tasks)...);

    if (!exceptions.empty())
    {
      throw AggregateException{exceptions};
    }
  }

  
  template <typename... TTask>
  int WaitAny(TTask&... tasks)
  {
    TaskCompletionSource<int> anyTcs;
    auto taskIndex = 0;
    (Detail::waitAnyTask(tasks, anyTcs, taskIndex++), ...);
    return anyTcs.GetTask().Result();   
  }

  template <typename... TTask>
  Task<int> WhenAny(TTask&... tasks)
  {
    TaskCompletionSource<int> anyTcs;
    auto taskIndex = 0;
    (Detail::waitAnyTask(tasks, anyTcs, taskIndex++), ...);
    return anyTcs.GetTask();   
  }

  //Identity method.
  template<typename TResult>
  Task<TResult> TaskFromResult(TResult&& taskResult)
  {
    //TODO: Detect invalid values.
    TaskCompletionSource<TResult> tcs;
    tcs.SetResult(std::forward<TResult>(taskResult));
    return tcs.GetTask();
  }
   //Identity method.

  template<typename TResult>
  Task<TResult> TaskFromResult(const TResult& taskResult)
  {
    //TODO: Detect invalid values.
    TaskCompletionSource<TResult> tcs;
    tcs.SetResult(taskResult);
    return tcs.GetTask();
  }

  namespace Detail
  {
    Task<void> getCompletedTask()
    {
      TaskCompletionSource<void> _tcs;
      _tcs.SetResult();
      return _tcs.GetTask();
    }
  }

  Task<void> GetCompletedTask()
  {
    static Task<void> _completedTask = Detail::getCompletedTask();    
    return _completedTask;
  }

  template<typename TResult>
  Task<TResult> TaskFromException(std::exception_ptr exception)
  {
    TaskCompletionSource<TResult> tcs;
    tcs.SetException(exception);
    return tcs.GetTask();
  }

  template<typename TResult>
  Task<TResult> TaskFromCanceled()
  {
    TaskCompletionSource<TResult> tcs;
    tcs.SetCanceled();
    return tcs.GetTask();
  }

  template<typename TSource, typename TMapFunc>
  auto Fmap(Task<TSource> srcTask, TMapFunc mapFunc)->Task<decltype(mapFunc(srcTask.Result()))>
  {    
    co_return mapFunc(co_await srcTask);    
  }

  template<typename TSource, typename TMapFunc>
  auto FBind(Task<TSource> srcTask, TMapFunc mapFunc)->Task<decltype(mapFunc(srcTask.Result()).Result)>
  {    
    co_return mapFunc(co_await srcTask);
    
  }
}