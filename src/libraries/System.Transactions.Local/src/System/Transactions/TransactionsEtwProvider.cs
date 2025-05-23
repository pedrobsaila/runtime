// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Diagnostics.Tracing;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Transactions.Oletx;

namespace System.Transactions
{
    internal enum EnlistmentType
    {
        Volatile = 0,
        Durable = 1,
        PromotableSinglePhase = 2
    }

    internal enum NotificationCall
    {
        // IEnlistmentNotification
        Prepare = 0,
        Commit = 1,
        Rollback = 2,
        InDoubt = 3,
        // ISinglePhaseNotification
        SinglePhaseCommit = 4,
        // IPromotableSinglePhaseNotification
        Promote = 5
    }

    internal enum EnlistmentCallback
    {
        Done = 0,
        Prepared = 1,
        ForceRollback = 2,
        Committed = 3,
        Aborted = 4,
        InDoubt = 5
    }

    internal enum TransactionScopeResult
    {
        CreatedTransaction = 0,
        UsingExistingCurrent = 1,
        TransactionPassed = 2,
        DependentTransactionPassed = 3,
        NoTransaction = 4
    }

    internal enum TransactionExceptionType
    {
        InvalidOperationException = 0,
        TransactionAbortedException = 1,
        TransactionException = 2,
        TransactionInDoubtException = 3,
        TransactionManagerCommunicationException = 4,
        UnrecognizedRecoveryInformation = 5
    }

    internal enum TraceSourceType
    {
        TraceSourceBase = 0,
        TraceSourceLtm = 1,
        TraceSourceOleTx = 2
    }
    /// <summary>Provides an event source for tracing Transactions information.</summary>
    [EventSource(
        Name = "System.Transactions.TransactionsEventSource",
        Guid = "8ac2d80a-1f1a-431b-ace4-bff8824aef0b")]
    internal sealed class TransactionsEtwProvider : EventSource
    {
        /// <summary>
        /// Defines the singleton instance for the Transactions ETW provider.
        /// The Transactions provider GUID is {8ac2d80a-1f1a-431b-ace4-bff8824aef0b}.
        /// </summary>
        ///


        internal static readonly TransactionsEtwProvider Log = new TransactionsEtwProvider();
        /// <summary>Prevent external instantiation.  All logging should go through the Log instance.</summary>
        private TransactionsEtwProvider() { }

        /// <summary>Enabled for all keywords.</summary>
        private const EventKeywords ALL_KEYWORDS = (EventKeywords)(-1);

        //-----------------------------------------------------------------------------------
        //
        // Transactions Event IDs (must be unique)
        //

        /// <summary>The event ID for configured default timeout adjusted event.</summary>
        private const int CONFIGURED_DEFAULT_TIMEOUT_ADJUSTED_EVENTID = 1;
        /// <summary>The event ID for the enlistment abort event.</summary>
        private const int ENLISTMENT_ABORTED_EVENTID = 2;
        /// <summary>The event ID for the enlistment commit event.</summary>
        private const int ENLISTMENT_COMMITTED_EVENTID = 3;
        /// <summary>The event ID for the enlistment done event.</summary>
        private const int ENLISTMENT_DONE_EVENTID = 4;
        /// <summary>The event ID for the enlistment status.</summary>
        private const int ENLISTMENT_LTM_EVENTID = 5;
        /// <summary>The event ID for the enlistment forcerollback event.</summary>
        private const int ENLISTMENT_FORCEROLLBACK_EVENTID = 6;
        /// <summary>The event ID for the enlistment indoubt event.</summary>
        private const int ENLISTMENT_INDOUBT_EVENTID = 7;
        /// <summary>The event ID for the enlistment prepared event.</summary>
        private const int ENLISTMENT_PREPARED_EVENTID = 8;
        /// <summary>The event ID for exception consumed event.</summary>
        private const int EXCEPTION_CONSUMED_BASE_EVENTID = 9;
        /// <summary>The event ID for exception consumed event.</summary>
        private const int EXCEPTION_CONSUMED_LTM_EVENTID = 10;
        /// <summary>The event ID for method enter event.</summary>
        private const int METHOD_ENTER_LTM_EVENTID = 11;
        /// <summary>The event ID for method exit event.</summary>
        private const int METHOD_EXIT_LTM_EVENTID = 12;
        /// <summary>The event ID for method enter event.</summary>
        private const int METHOD_ENTER_BASE_EVENTID = 13;
        /// <summary>The event ID for method exit event.</summary>
        private const int METHOD_EXIT_BASE_EVENTID = 14;
        /// <summary>The event ID for method enter event.</summary>
        private const int METHOD_ENTER_OLETX_EVENTID = 15;
        /// <summary>The event ID for method exit event.</summary>
        private const int METHOD_EXIT_OLETX_EVENTID = 16;
        /// <summary>The event ID for transaction aborted event.</summary>
        private const int TRANSACTION_ABORTED_LTM_EVENTID = 17;
        /// <summary>The event ID for the transaction clone create event.</summary>
        private const int TRANSACTION_CLONECREATE_EVENTID = 18;
        /// <summary>The event ID for the transaction commit event.</summary>
        private const int TRANSACTION_COMMIT_LTM_EVENTID = 19;
        /// <summary>The event ID for transaction committed event.</summary>
        private const int TRANSACTION_COMMITTED_LTM_EVENTID = 20;
        /// <summary>The event ID for when we encounter a new Transactions object that hasn't had its name traced to the trace file.</summary>
        private const int TRANSACTION_CREATED_LTM_EVENTID = 21;
        /// <summary>The event ID for the transaction dependent clone complete event.</summary>
        private const int TRANSACTION_DEPENDENT_CLONE_COMPLETE_LTM_EVENTID = 22;
        /// <summary>The event ID for the transaction exception event.</summary>
        private const int TRANSACTION_EXCEPTION_LTM_EVENTID = 23;
        /// <summary>The event ID for the transaction exception event.</summary>
        private const int TRANSACTION_EXCEPTION_BASE_EVENTID = 24;
        /// <summary>The event ID for transaction indoubt event.</summary>
        private const int TRANSACTION_INDOUBT_LTM_EVENTID = 25;
        /// <summary>The event ID for the transaction invalid operation event.</summary>
        private const int TRANSACTION_INVALID_OPERATION_EVENTID = 26;
        /// <summary>The event ID for transaction promoted event.</summary>
        private const int TRANSACTION_PROMOTED_EVENTID = 27;
        /// <summary>The event ID for the transaction rollback event.</summary>
        private const int TRANSACTION_ROLLBACK_LTM_EVENTID = 28;
        /// <summary>The event ID for the transaction serialized event.</summary>
        private const int TRANSACTION_SERIALIZED_EVENTID = 29;
        /// <summary>The event ID for transaction timeout event.</summary>
        private const int TRANSACTION_TIMEOUT_EVENTID = 30;
        /// <summary>The event ID for transactionmanager recovery complete event.</summary>
        private const int TRANSACTIONMANAGER_RECOVERY_COMPLETE_EVENTID = 31;
        /// <summary>The event ID for transactionmanager reenlist event.</summary>
        private const int TRANSACTIONMANAGER_REENLIST_EVENTID = 32;
        /// <summary>The event ID for transactionscope created event.</summary>
        private const int TRANSACTIONSCOPE_CREATED_EVENTID = 33;
        /// <summary>The event ID for transactionscope current changed event.</summary>
        private const int TRANSACTIONSCOPE_CURRENT_CHANGED_EVENTID = 34;
        /// <summary>The event ID for transactionscope nested incorrectly event.</summary>
        private const int TRANSACTIONSCOPE_DISPOSED_EVENTID = 35;
        /// <summary>The event ID for transactionscope incomplete event.</summary>
        private const int TRANSACTIONSCOPE_INCOMPLETE_EVENTID = 36;
        /// <summary>The event ID for transactionscope internal error event.</summary>
        private const int INTERNAL_ERROR_EVENTID = 37;
        /// <summary>The event ID for transactionscope nested incorrectly event.</summary>
        private const int TRANSACTIONSCOPE_NESTED_INCORRECTLY_EVENTID = 38;
        /// <summary>The event ID for transactionscope timeout event.</summary>
        private const int TRANSACTIONSCOPE_TIMEOUT_EVENTID = 39;
        /// <summary>The event ID for enlistment event.</summary>
        private const int TRANSACTIONSTATE_ENLIST_EVENTID = 40;

        /// <summary>The event ID for the transaction commit event.</summary>
        private const int TRANSACTION_COMMIT_OLETX_EVENTID = 41;
        /// <summary>The event ID for the transaction rollback event.</summary>
        private const int TRANSACTION_ROLLBACK_OLETX_EVENTID = 42;
        /// <summary>The event ID for exception consumed event.</summary>
        private const int EXCEPTION_CONSUMED_OLETX_EVENTID = 43;
        /// <summary>The event ID for transaction committed event.</summary>
        private const int TRANSACTION_COMMITTED_OLETX_EVENTID = 44;
        /// <summary>The event ID for transaction aborted event.</summary>
        private const int TRANSACTION_ABORTED_OLETX_EVENTID = 45;
        /// <summary>The event ID for transaction indoubt event.</summary>
        private const int TRANSACTION_INDOUBT_OLETX_EVENTID = 46;
        /// <summary>The event ID for the transaction dependent clone complete event.</summary>
        private const int TRANSACTION_DEPENDENT_CLONE_COMPLETE_OLETX_EVENTID = 47;
        /// <summary>The event ID for the transaction dependent clone complete event.</summary>
        private const int TRANSACTION_DEPENDENT_CLONE_CREATE_LTM_EVENTID = 48;
        /// <summary>The event ID for the transaction dependent clone complete event.</summary>
        private const int TRANSACTION_DEPENDENT_CLONE_CREATE_OLETX_EVENTID = 49;
        /// <summary>The event ID for the transaction deserialized event.</summary>
        private const int TRANSACTION_DESERIALIZED_EVENTID = 50;
        /// <summary>The event ID for when we encounter a new Transactions object that hasn't had its name traced to the trace file.</summary>
        private const int TRANSACTION_CREATED_OLETX_EVENTID = 51;

        /// <summary>The event ID for the enlistment status.</summary>
        private const int ENLISTMENT_OLETX_EVENTID = 52;
        /// <summary>The event ID for the enlistment callback positive event.</summary>
        private const int ENLISTMENT_CALLBACK_POSITIVE_EVENTID = 53;
        /// <summary>The event ID for the enlistment callback positive event.</summary>
        private const int ENLISTMENT_CALLBACK_NEGATIVE_EVENTID = 54;
        /// <summary>The event ID for when we create an enlistment.</summary>
        private const int ENLISTMENT_CREATED_LTM_EVENTID = 55;
        /// <summary>The event ID for when we create an enlistment.</summary>
        private const int ENLISTMENT_CREATED_OLETX_EVENTID = 56;

        /// <summary>The event ID for transactionmanager reenlist event.</summary>
        private const int TRANSACTIONMANAGER_CREATE_OLETX_EVENTID = 57;

        //-----------------------------------------------------------------------------------
        //
        // Transactions Events
        //

        private const string NullInstance = "(null)";
        //-----------------------------------------------------------------------------------
        //
        // Transactions Events
        //
        [NonEvent]
        public static string IdOf(object? value) => value != null ? value.GetType().Name + "#" + GetHashCode(value) : NullInstance;

        [NonEvent]
        public static int GetHashCode(object? value) => value?.GetHashCode() ?? 0;

        #region Transaction Creation
        [NonEvent]
        internal void TransactionCreated(TraceSourceType traceSource, TransactionTraceIdentifier txTraceId, string? type)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionCreatedLtm(txTraceId.TransactionIdentifier, type);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionCreatedOleTx(txTraceId.TransactionIdentifier, type);
                }
            }
        }

        [Event(TRANSACTION_CREATED_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.Create, Message = "Transaction Created (LTM). ID is {0}, type is {1}")]
        private void TransactionCreatedLtm(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_CREATED_LTM_EVENTID, transactionIdentifier, type);
        }

        [Event(TRANSACTION_CREATED_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.Create, Message = "Transaction Created (OLETX). ID is {0}, type is {1}")]
        private void TransactionCreatedOleTx(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_CREATED_OLETX_EVENTID, transactionIdentifier, type);
        }
        #endregion

        #region Transaction Clone Create
        /// <summary>Trace an event when a new transaction is clone created.</summary>
        /// <param name="transaction">The transaction that was clone created.</param>
        /// <param name="type">The type of transaction.</param>
        [NonEvent]
        internal void TransactionCloneCreate(Transaction transaction, string type)
        {
            Debug.Assert(transaction != null, "Transaction needed for the ETW event.");

            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                if (transaction != null && transaction.TransactionTraceId.TransactionIdentifier != null)
                    TransactionCloneCreate(transaction.TransactionTraceId.TransactionIdentifier, type);
                else
                    TransactionCloneCreate(string.Empty, type);
            }
        }

        [Event(TRANSACTION_CLONECREATE_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.CloneCreate, Message = "Transaction Clone Created. ID is {0}, type is {1}")]
        private void TransactionCloneCreate(string transactionIdentifier, string type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_CLONECREATE_EVENTID, transactionIdentifier, type);
        }
        #endregion

        #region Transaction Serialized
        [NonEvent]
        internal void TransactionSerialized(TransactionTraceIdentifier transactionTraceId)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                TransactionSerialized(transactionTraceId.TransactionIdentifier);
            }
        }

        [Event(TRANSACTION_SERIALIZED_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.Serialized, Message = "Transaction Serialized. ID is {0}")]
        private void TransactionSerialized(string transactionIdentifier)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_SERIALIZED_EVENTID, transactionIdentifier);
        }
        #endregion

        #region Transaction Deserialized
        [NonEvent]
        internal void TransactionDeserialized(TransactionTraceIdentifier transactionTraceId)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                TransactionDeserialized(transactionTraceId.TransactionIdentifier);
            }
        }

        [Event(TRANSACTION_DESERIALIZED_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.Transaction, Opcode = Opcodes.Serialized, Message = "Transaction Deserialized. ID is {0}")]
        private void TransactionDeserialized(string transactionIdentifier)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_DESERIALIZED_EVENTID, transactionIdentifier);
        }
        #endregion

        #region Transaction Exception
        /// <summary>Trace an event when an exception happens.</summary>
        /// <param name="traceSource">trace source</param>
        /// <param name="type">The type of transaction.</param>
        /// <param name="message">The message for the exception.</param>
        /// <param name="innerExceptionStr">The inner exception.</param>
        [NonEvent]
        internal void TransactionExceptionTrace(TraceSourceType traceSource, TransactionExceptionType type, string? message, string? innerExceptionStr)
        {
            if (IsEnabled(EventLevel.Error, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceBase)
                {
                    TransactionExceptionBase(type.ToString(), message, innerExceptionStr);
                }
                else
                {
                    TransactionExceptionLtm(type.ToString(), message, innerExceptionStr);
                }
            }
        }

        /// <summary>Trace an event when an exception happens.</summary>
        /// <param name="type">The type of transaction.</param>
        /// <param name="message">The message for the exception.</param>
        /// <param name="innerExceptionStr">The inner exception.</param>
        [NonEvent]
        internal void TransactionExceptionTrace(TransactionExceptionType type, string? message, string? innerExceptionStr)
        {
            if (IsEnabled(EventLevel.Error, ALL_KEYWORDS))
            {
                TransactionExceptionLtm(type.ToString(), message, innerExceptionStr);
            }
        }

        [Event(TRANSACTION_EXCEPTION_BASE_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Error, Task = Tasks.TransactionException, Message = "Transaction Exception. Type is {0}, message is {1}, InnerException is {2}")]
        private void TransactionExceptionBase(string? type, string? message, string? innerExceptionStr)
        {
            SetActivityId(string.Empty);
            WriteEvent(TRANSACTION_EXCEPTION_BASE_EVENTID, type, message, innerExceptionStr);
        }

        [Event(TRANSACTION_EXCEPTION_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Error, Task = Tasks.TransactionException, Message = "Transaction Exception. Type is {0}, message is {1}, InnerException is {2}")]
        private void TransactionExceptionLtm(string? type, string? message, string? innerExceptionStr)
        {
            SetActivityId(string.Empty);
            WriteEvent(TRANSACTION_EXCEPTION_LTM_EVENTID, type, message, innerExceptionStr);
        }
        #endregion

        #region Transaction Invalid Operation
        /// <summary>Trace an event when an invalid operation happened on a transaction.</summary>
        /// <param name="type">The type of transaction.</param>
        /// <param name="operation">The operationont the transaction.</param>
        [NonEvent]
        internal void InvalidOperation(string? type, string? operation)
        {
            if (IsEnabled(EventLevel.Error, ALL_KEYWORDS))
            {
                TransactionInvalidOperation(
                    string.Empty, type, operation);
            }
        }
        [Event(TRANSACTION_INVALID_OPERATION_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Error, Task = Tasks.Transaction, Opcode = Opcodes.InvalidOperation, Message = "Transaction Invalid Operation. ID is {0}, type is {1} and operation is {2}")]
        private void TransactionInvalidOperation(string? transactionIdentifier, string? type, string? operation)
        {
            SetActivityId(string.Empty);
            WriteEvent(TRANSACTION_INVALID_OPERATION_EVENTID, transactionIdentifier, type, operation);
        }
        #endregion

        #region Transaction Rollback
        [NonEvent]
        internal void TransactionRollback(TraceSourceType traceSource, TransactionTraceIdentifier txTraceId, string? type)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionRollbackLtm(txTraceId.TransactionIdentifier, type);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionRollbackOleTx(txTraceId.TransactionIdentifier, type);
                }
            }
        }

        [Event(TRANSACTION_ROLLBACK_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Warning, Task = Tasks.Transaction, Opcode = Opcodes.Rollback, Message = "Transaction LTM Rollback. ID is {0}, type is {1}")]
        private void TransactionRollbackLtm(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_ROLLBACK_LTM_EVENTID, transactionIdentifier, type);
        }

        [Event(TRANSACTION_ROLLBACK_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Warning, Task = Tasks.Transaction, Opcode = Opcodes.Rollback, Message = "Transaction OleTx Rollback. ID is {0}, type is {1}")]
        private void TransactionRollbackOleTx(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_ROLLBACK_OLETX_EVENTID, transactionIdentifier, type);
        }
        #endregion

        #region Transaction Dependent Clone Create
        [NonEvent]
        internal void TransactionDependentCloneCreate(TraceSourceType traceSource, TransactionTraceIdentifier txTraceId, DependentCloneOption option)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionDependentCloneCreateLtm(txTraceId.TransactionIdentifier, option.ToString());
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionDependentCloneCreateOleTx(txTraceId.TransactionIdentifier, option.ToString());
                }
            }
        }

        [Event(TRANSACTION_DEPENDENT_CLONE_CREATE_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.DependentCloneComplete, Message = "Transaction Dependent Clone Created (LTM). ID is {0}, option is {1}")]
        private void TransactionDependentCloneCreateLtm(string transactionIdentifier, string? option)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_DEPENDENT_CLONE_CREATE_LTM_EVENTID, transactionIdentifier, option);
        }

        [Event(TRANSACTION_DEPENDENT_CLONE_CREATE_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.DependentCloneComplete, Message = "Transaction Dependent Clone Created (OLETX). ID is {0}, option is {1}")]
        private void TransactionDependentCloneCreateOleTx(string transactionIdentifier, string? option)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_DEPENDENT_CLONE_CREATE_OLETX_EVENTID, transactionIdentifier, option);
        }
        #endregion

        #region Transaction Dependent Clone Complete
        [NonEvent]
        internal void TransactionDependentCloneComplete(TraceSourceType traceSource, TransactionTraceIdentifier txTraceId, string? type)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionDependentCloneCompleteLtm(txTraceId.TransactionIdentifier, type);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionDependentCloneCompleteOleTx(txTraceId.TransactionIdentifier, type);
                }
            }
        }

        [Event(TRANSACTION_DEPENDENT_CLONE_COMPLETE_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.DependentCloneComplete, Message = "Transaction Dependent Clone Completed (LTM). ID is {0}, type is {1}")]
        private void TransactionDependentCloneCompleteLtm(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_DEPENDENT_CLONE_COMPLETE_LTM_EVENTID, transactionIdentifier, type);
        }

        [Event(TRANSACTION_DEPENDENT_CLONE_COMPLETE_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.DependentCloneComplete, Message = "Transaction Dependent Clone Completed (OLETX). ID is {0}, type is {1}")]
        private void TransactionDependentCloneCompleteOleTx(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_DEPENDENT_CLONE_COMPLETE_OLETX_EVENTID, transactionIdentifier, type);
        }
        #endregion

        #region Transaction Commit
        [NonEvent]
        internal void TransactionCommit(TraceSourceType traceSource, TransactionTraceIdentifier txTraceId, string? type)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionCommitLtm(txTraceId.TransactionIdentifier, type);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionCommitOleTx(txTraceId.TransactionIdentifier, type);
                }
            }
        }

        [Event(TRANSACTION_COMMIT_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Transaction, Opcode = Opcodes.Commit, Message = "Transaction LTM Commit: ID is {0}, type is {1}")]
        private void TransactionCommitLtm(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_COMMIT_LTM_EVENTID, transactionIdentifier, type);
        }

        [Event(TRANSACTION_COMMIT_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.Transaction, Opcode = Opcodes.Commit, Message = "Transaction OleTx Commit: ID is {0}, type is {1}")]
        private void TransactionCommitOleTx(string transactionIdentifier, string? type)
        {
            SetActivityId(transactionIdentifier);
            WriteEvent(TRANSACTION_COMMIT_OLETX_EVENTID, transactionIdentifier, type);
        }
        #endregion

        #region Enlistment
        [NonEvent]
        internal void EnlistmentStatus(TraceSourceType traceSource, EnlistmentTraceIdentifier enlistmentTraceId, NotificationCall notificationCall)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    EnlistmentStatusLtm(enlistmentTraceId.EnlistmentIdentifier, notificationCall.ToString());
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    EnlistmentStatusOleTx(enlistmentTraceId.EnlistmentIdentifier, notificationCall.ToString());
                }
            }
        }

        [Event(ENLISTMENT_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Enlistment, Message = "Enlistment status (LTM): ID is {0}, notificationcall is {1}")]
        private void EnlistmentStatusLtm(int enlistmentIdentifier, string notificationCall)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_LTM_EVENTID, enlistmentIdentifier, notificationCall);
        }

        [Event(ENLISTMENT_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.Enlistment, Message = "Enlistment status (OLETX): ID is {0}, notificationcall is {1}")]
        private void EnlistmentStatusOleTx(int enlistmentIdentifier, string notificationCall)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_OLETX_EVENTID, enlistmentIdentifier, notificationCall);
        }
        #endregion

        #region Enlistment Creation
        [NonEvent]
        internal void EnlistmentCreated(TraceSourceType traceSource, EnlistmentTraceIdentifier enlistmentTraceId, EnlistmentType enlistmentType, EnlistmentOptions enlistmentOptions)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    EnlistmentCreatedLtm(enlistmentTraceId.EnlistmentIdentifier, enlistmentType.ToString(), enlistmentOptions.ToString());
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    EnlistmentCreatedOleTx(enlistmentTraceId.EnlistmentIdentifier, enlistmentType.ToString(), enlistmentOptions.ToString());
                }
            }
        }

        [Event(ENLISTMENT_CREATED_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Informational, Task = Tasks.Enlistment, Opcode = Opcodes.Create, Message = "Enlistment Created (LTM). ID is {0}, type is {1}, options is {2}")]
        private void EnlistmentCreatedLtm(int enlistmentIdentifier, string enlistmentType, string enlistmentOptions)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_CREATED_LTM_EVENTID, enlistmentIdentifier, enlistmentType, enlistmentOptions);
        }

        [Event(ENLISTMENT_CREATED_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Informational, Task = Tasks.Enlistment, Opcode = Opcodes.Create, Message = "Enlistment Created (OLETX). ID is {0}, type is {1}, options is {2}")]
        private void EnlistmentCreatedOleTx(int enlistmentIdentifier, string enlistmentType, string enlistmentOptions)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_CREATED_OLETX_EVENTID, enlistmentIdentifier, enlistmentType, enlistmentOptions);
        }
        #endregion

        #region Enlistment Done
        /// <summary>Trace an event for enlistment done.</summary>
        /// <param name="enlistment">The enlistment done.</param>
        [NonEvent]
        internal void EnlistmentDone(InternalEnlistment enlistment)
        {
            Debug.Assert(enlistment != null, "Enlistment needed for the ETW event.");

            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (enlistment != null && enlistment.EnlistmentTraceId.EnlistmentIdentifier != 0)
                    EnlistmentDone(enlistment.EnlistmentTraceId.EnlistmentIdentifier);
                else
                    EnlistmentDone(0);
            }
        }

        [Event(ENLISTMENT_DONE_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Enlistment, Opcode = Opcodes.Done, Message = "Enlistment.Done: ID is {0}")]
        private void EnlistmentDone(int enlistmentIdentifier)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_DONE_EVENTID, enlistmentIdentifier);
        }
        #endregion

        #region Enlistment Prepared
        /// <summary>Trace an event for enlistment prepared.</summary>
        /// <param name="enlistment">The enlistment prepared.</param>
        [NonEvent]
        internal void EnlistmentPrepared(InternalEnlistment enlistment)
        {
            Debug.Assert(enlistment != null, "Enlistment needed for the ETW event.");

            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (enlistment != null && enlistment.EnlistmentTraceId.EnlistmentIdentifier != 0)
                    EnlistmentPrepared(enlistment.EnlistmentTraceId.EnlistmentIdentifier);
                else
                    EnlistmentPrepared(0);
            }
        }

        [Event(ENLISTMENT_PREPARED_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Enlistment, Opcode = Opcodes.Prepared, Message = "PreparingEnlistment.Prepared: ID is {0}")]
        private void EnlistmentPrepared(int enlistmentIdentifier)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_PREPARED_EVENTID, enlistmentIdentifier);
        }
        #endregion

        #region Enlistment ForceRollback
        /// <summary>Trace an enlistment that will forcerollback.</summary>
        /// <param name="enlistment">The enlistment to forcerollback.</param>
        [NonEvent]
        internal void EnlistmentForceRollback(InternalEnlistment enlistment)
        {
            Debug.Assert(enlistment != null, "Enlistment needed for the ETW event.");

            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                if (enlistment != null && enlistment.EnlistmentTraceId.EnlistmentIdentifier != 0)
                    EnlistmentForceRollback(enlistment.EnlistmentTraceId.EnlistmentIdentifier);
                else
                    EnlistmentForceRollback(0);
            }
        }

        [Event(ENLISTMENT_FORCEROLLBACK_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Warning, Task = Tasks.Enlistment, Opcode = Opcodes.ForceRollback, Message = "Enlistment forceRollback: ID is {0}")]
        private void EnlistmentForceRollback(int enlistmentIdentifier)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_FORCEROLLBACK_EVENTID, enlistmentIdentifier);
        }
        #endregion

        #region Enlistment Aborted
        /// <summary>Trace an enlistment that aborted.</summary>
        /// <param name="enlistment">The enlistment aborted.</param>
        [NonEvent]
        internal void EnlistmentAborted(InternalEnlistment enlistment)
        {
            Debug.Assert(enlistment != null, "Enlistment needed for the ETW event.");

            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                if (enlistment != null && enlistment.EnlistmentTraceId.EnlistmentIdentifier != 0)
                    EnlistmentAborted(enlistment.EnlistmentTraceId.EnlistmentIdentifier);
                else
                    EnlistmentAborted(0);

            }
        }

        [Event(ENLISTMENT_ABORTED_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Warning, Task = Tasks.Enlistment, Opcode = Opcodes.Aborted, Message = "Enlistment SinglePhase Aborted: ID is {0}")]
        private void EnlistmentAborted(int enlistmentIdentifier)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_ABORTED_EVENTID, enlistmentIdentifier);
        }
        #endregion

        #region Enlistment Committed
        /// <summary>Trace an enlistment that committed.</summary>
        /// <param name="enlistment">The enlistment committed.</param>
        [NonEvent]
        internal void EnlistmentCommitted(InternalEnlistment enlistment)
        {
            Debug.Assert(enlistment != null, "Enlistment needed for the ETW event.");

            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (enlistment != null && enlistment.EnlistmentTraceId.EnlistmentIdentifier != 0)
                    EnlistmentCommitted(enlistment.EnlistmentTraceId.EnlistmentIdentifier);
                else
                    EnlistmentCommitted(0);
            }
        }

        [Event(ENLISTMENT_COMMITTED_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Enlistment, Opcode = Opcodes.Committed, Message = "Enlistment Committed: ID is {0}")]
        private void EnlistmentCommitted(int enlistmentIdentifier)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_COMMITTED_EVENTID, enlistmentIdentifier);
        }
        #endregion

        #region Enlistment InDoubt
        /// <summary>Trace an enlistment that InDoubt.</summary>
        /// <param name="enlistment">The enlistment Indoubt.</param>
        [NonEvent]
        internal void EnlistmentInDoubt(InternalEnlistment enlistment)
        {
            Debug.Assert(enlistment != null, "Enlistment needed for the ETW event.");

            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                if (enlistment != null && enlistment.EnlistmentTraceId.EnlistmentIdentifier != 0)
                    EnlistmentInDoubt(enlistment.EnlistmentTraceId.EnlistmentIdentifier);
                else
                    EnlistmentInDoubt(0);
            }
        }

        [Event(ENLISTMENT_INDOUBT_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Warning, Task = Tasks.Enlistment, Opcode = Opcodes.InDoubt, Message = "Enlistment SinglePhase InDoubt: ID is {0}")]
        private void EnlistmentInDoubt(int enlistmentIdentifier)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_INDOUBT_EVENTID, enlistmentIdentifier);
        }
        #endregion

        #region Enlistment Callback Positive
        [NonEvent]
        internal void EnlistmentCallbackPositive(EnlistmentTraceIdentifier enlistmentTraceIdentifier, EnlistmentCallback callback)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                EnlistmentCallbackPositive(enlistmentTraceIdentifier.EnlistmentIdentifier, callback.ToString());
            }
        }

        [Event(ENLISTMENT_CALLBACK_POSITIVE_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.Enlistment, Opcode = Opcodes.CallbackPositive, Message = "Enlistment callback positive: ID is {0}, callback is {1}")]
        private void EnlistmentCallbackPositive(int enlistmentIdentifier, string? callback)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_CALLBACK_POSITIVE_EVENTID, enlistmentIdentifier, callback);
        }
        #endregion

        #region Enlistment Callback Negative
        [NonEvent]
        internal void EnlistmentCallbackNegative(EnlistmentTraceIdentifier enlistmentTraceIdentifier, EnlistmentCallback callback)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                EnlistmentCallbackNegative(enlistmentTraceIdentifier.EnlistmentIdentifier, callback.ToString());
            }
        }

        [Event(ENLISTMENT_CALLBACK_NEGATIVE_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Warning, Task = Tasks.Enlistment, Opcode = Opcodes.CallbackNegative, Message = "Enlistment callback negative: ID is {0}, callback is {1}")]
        private void EnlistmentCallbackNegative(int enlistmentIdentifier, string? callback)
        {
            SetActivityId(string.Empty);
            WriteEvent(ENLISTMENT_CALLBACK_NEGATIVE_EVENTID, enlistmentIdentifier, callback);
        }
        #endregion

        #region Method Enter
        /// <summary>Trace an event when enter a method.</summary>
        /// <param name="traceSource"> trace source</param>
        /// <param name="thisOrContextObject">'this', or another object that serves to provide context for the operation.</param>
        /// <param name="methodname">The name of method.</param>
        [NonEvent]
        internal void MethodEnter(TraceSourceType traceSource, object? thisOrContextObject, [CallerMemberName] string? methodname = null)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    MethodEnterTraceLtm(IdOf(thisOrContextObject), methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceBase)
                {
                    MethodEnterTraceBase(IdOf(thisOrContextObject), methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    MethodEnterTraceDistributed(IdOf(thisOrContextObject), methodname);
                }
            }
        }

        /// <summary>Trace an event when enter a method.</summary>
        /// <param name="traceSource"> trace source</param>
        /// <param name="methodname">The name of method.</param>
        [NonEvent]
        internal void MethodEnter(TraceSourceType traceSource, [CallerMemberName] string? methodname = null)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    MethodEnterTraceLtm(string.Empty, methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceBase)
                {
                    MethodEnterTraceBase(string.Empty, methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    MethodEnterTraceDistributed(string.Empty, methodname);
                }
            }
        }

        [Event(METHOD_ENTER_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Method, Opcode = Opcodes.Enter, Message = "Enter method : {0}.{1}")]
        private void MethodEnterTraceLtm(string thisOrContextObject, string? methodname)
        {
            SetActivityId(string.Empty);
            WriteEvent(METHOD_ENTER_LTM_EVENTID, thisOrContextObject, methodname);
        }
        [Event(METHOD_ENTER_BASE_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Verbose, Task = Tasks.Method, Opcode = Opcodes.Enter, Message = "Enter method : {0}.{1}")]
        private void MethodEnterTraceBase(string thisOrContextObject, string? methodname)
        {
            SetActivityId(string.Empty);
            WriteEvent(METHOD_ENTER_BASE_EVENTID, thisOrContextObject, methodname);
        }
        [Event(METHOD_ENTER_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.Method, Opcode = Opcodes.Enter, Message = "Enter method : {0}.{1}")]
        private void MethodEnterTraceDistributed(string thisOrContextObject, string? methodname)
        {
            SetActivityId(string.Empty);
            WriteEvent(METHOD_ENTER_OLETX_EVENTID, thisOrContextObject, methodname);
        }
        #endregion

        #region Method Exit
        /// <summary>Trace an event when enter a method.</summary>
        /// <param name="traceSource"> trace source</param>
        /// <param name="thisOrContextObject">'this', or another object that serves to provide context for the operation.</param>
        /// <param name="methodname">The name of method.</param>
        [NonEvent]
        internal void MethodExit(TraceSourceType traceSource, object? thisOrContextObject, [CallerMemberName] string? methodname = null)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    MethodExitTraceLtm(IdOf(thisOrContextObject), methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceBase)
                {
                    MethodExitTraceBase(IdOf(thisOrContextObject), methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    MethodExitTraceDistributed(IdOf(thisOrContextObject), methodname);
                }
            }
        }

        /// <summary>Trace an event when enter a method.</summary>
        /// <param name="traceSource"> trace source</param>
        /// <param name="methodname">The name of method.</param>
        [NonEvent]
        internal void MethodExit(TraceSourceType traceSource, [CallerMemberName] string? methodname = null)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    MethodExitTraceLtm(string.Empty, methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceBase)
                {
                    MethodExitTraceBase(string.Empty, methodname);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    MethodExitTraceDistributed(string.Empty, methodname);
                }
            }
        }

        [Event(METHOD_EXIT_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Method, Opcode = Opcodes.Exit, Message = "Exit method: {0}.{1}")]
        private void MethodExitTraceLtm(string thisOrContextObject, string? methodname)
        {
            SetActivityId(string.Empty);
            WriteEvent(METHOD_EXIT_LTM_EVENTID, thisOrContextObject, methodname);
        }
        [Event(METHOD_EXIT_BASE_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Verbose, Task = Tasks.Method, Opcode = Opcodes.Exit, Message = "Exit method: {0}.{1}")]
        private void MethodExitTraceBase(string thisOrContextObject, string? methodname)
        {
            SetActivityId(string.Empty);
            WriteEvent(METHOD_EXIT_BASE_EVENTID, thisOrContextObject, methodname);
        }
        [Event(METHOD_EXIT_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.Method, Opcode = Opcodes.Exit, Message = "Exit method: {0}.{1}")]
        private void MethodExitTraceDistributed(string thisOrContextObject, string? methodname)
        {
            SetActivityId(string.Empty);
            WriteEvent(METHOD_EXIT_OLETX_EVENTID, thisOrContextObject, methodname);
        }

        #endregion

        #region Exception Consumed
        /// <summary>Trace an event when exception consumed.</summary>
        /// <param name="traceSource">trace source</param>
        /// <param name="exception">The exception.</param>
        [NonEvent]
        internal void ExceptionConsumed(TraceSourceType traceSource, Exception exception)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                switch (traceSource)
                {
                    case TraceSourceType.TraceSourceBase:
                        ExceptionConsumedBase(exception.ToString());
                        return;
                    case TraceSourceType.TraceSourceLtm:
                        ExceptionConsumedLtm(exception.ToString());
                        return;
                    case TraceSourceType.TraceSourceOleTx:
                        ExceptionConsumedOleTx(exception.ToString());
                        return;
                }
            }
        }
        /// <summary>Trace an event when exception consumed.</summary>
        /// <param name="exception">The exception.</param>
        [NonEvent]
        internal void ExceptionConsumed(Exception exception)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                ExceptionConsumedLtm(exception.ToString());
            }
        }

        [Event(EXCEPTION_CONSUMED_BASE_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Verbose, Opcode = Opcodes.ExceptionConsumed, Message = "Exception consumed: {0}")]
        private void ExceptionConsumedBase(string exceptionStr)
        {
            SetActivityId(string.Empty);
            WriteEvent(EXCEPTION_CONSUMED_BASE_EVENTID, exceptionStr);
        }
        [Event(EXCEPTION_CONSUMED_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Opcode = Opcodes.ExceptionConsumed, Message = "Exception consumed: {0}")]
        private void ExceptionConsumedLtm(string exceptionStr)
        {
            SetActivityId(string.Empty);
            WriteEvent(EXCEPTION_CONSUMED_LTM_EVENTID, exceptionStr);
        }
        [Event(EXCEPTION_CONSUMED_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Opcode = Opcodes.ExceptionConsumed, Message = "Exception consumed: {0}")]
        private void ExceptionConsumedOleTx(string exceptionStr)
        {
            SetActivityId(string.Empty);
            WriteEvent(EXCEPTION_CONSUMED_OLETX_EVENTID, exceptionStr);
        }
        #endregion

        #region OleTx TransactionManager Create
        [NonEvent]
        internal void OleTxTransactionManagerCreate(Type tmType, string? nodeName)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                OleTxTransactionManagerCreate(tmType.ToString(), nodeName);
            }
        }

        [Event(TRANSACTIONMANAGER_CREATE_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.TransactionManager, Opcode = Opcodes.Created, Message = "Created OleTx transaction manager, type is {0}, node name is {1}")]
        private void OleTxTransactionManagerCreate(string tmType, string? nodeName)
        {
            SetActivityId(string.Empty);
            WriteEvent(TRANSACTIONMANAGER_CREATE_OLETX_EVENTID, tmType, nodeName);
        }
        #endregion

        #region TransactionManager Reenlist
        /// <summary>Trace an event when reenlist transactionmanager.</summary>
        /// <param name="resourceManagerID">The resource manager ID.</param>
        [NonEvent]
        internal void TransactionManagerReenlist(Guid resourceManagerID)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                TransactionManagerReenlistTrace(resourceManagerID.ToString());
            }
        }

        [Event(TRANSACTIONMANAGER_REENLIST_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Informational, Task = Tasks.TransactionManager, Opcode = Opcodes.Reenlist, Message = "Reenlist in: {0}")]
        private void TransactionManagerReenlistTrace(string rmID)
        {
            SetActivityId(string.Empty);
            WriteEvent(TRANSACTIONMANAGER_REENLIST_EVENTID, rmID);
        }
        #endregion

        #region TransactionManager Recovery Complete
        /// <summary>Trace an event when transactionmanager recovery complete.</summary>
        /// <param name="resourceManagerID">The resource manager ID.</param>
        [NonEvent]
        internal void TransactionManagerRecoveryComplete(Guid resourceManagerID)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                TransactionManagerRecoveryComplete(resourceManagerID.ToString());
            }
        }

        [Event(TRANSACTIONMANAGER_RECOVERY_COMPLETE_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Informational, Task = Tasks.TransactionManager, Opcode = Opcodes.RecoveryComplete, Message = "Recovery complete: {0}")]
        private void TransactionManagerRecoveryComplete(string rmID)
        {
            SetActivityId(string.Empty);
            WriteEvent(TRANSACTIONMANAGER_RECOVERY_COMPLETE_EVENTID, rmID);
        }
        #endregion

        #region Configured Default Timeout Adjusted
        /// <summary>Trace an event when configured default timeout adjusted.</summary>
        [NonEvent]
        internal void ConfiguredDefaultTimeoutAdjusted()
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                ConfiguredDefaultTimeoutAdjustedTrace();
            }
        }

        [Event(CONFIGURED_DEFAULT_TIMEOUT_ADJUSTED_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Warning, Task = Tasks.ConfiguredDefaultTimeout, Opcode = Opcodes.Adjusted, Message = "Configured Default Timeout Adjusted")]
        private void ConfiguredDefaultTimeoutAdjustedTrace()
        {
            SetActivityId(string.Empty);
            WriteEvent(CONFIGURED_DEFAULT_TIMEOUT_ADJUSTED_EVENTID);
        }
        #endregion

        #region Transactionscope Created
        /// <summary>Trace an event when a transactionscope is created.</summary>
        /// <param name="transactionID">The transaction ID.</param>
        /// <param name="transactionScopeResult">The transaction scope result.</param>
        [NonEvent]
        internal void TransactionScopeCreated(TransactionTraceIdentifier transactionID, TransactionScopeResult transactionScopeResult)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                TransactionScopeCreated(transactionID.TransactionIdentifier ?? string.Empty, transactionScopeResult);
            }
        }

        [Event(TRANSACTIONSCOPE_CREATED_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Informational, Task = Tasks.TransactionScope, Opcode = Opcodes.Created, Message = "Transactionscope was created: Transaction ID is {0}, TransactionScope Result is {1}")]
        private void TransactionScopeCreated(string transactionID, TransactionScopeResult transactionScopeResult)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTIONSCOPE_CREATED_EVENTID, transactionID, transactionScopeResult);
        }
        #endregion

        #region Transactionscope Current Changed
        /// <summary>Trace an event when a transactionscope current transaction changed.</summary>
        /// <param name="currenttransactionID">The transaction ID.</param>
        /// <param name="newtransactionID">The new transaction ID.</param>
        [NonEvent]
        internal void TransactionScopeCurrentChanged(TransactionTraceIdentifier currenttransactionID, TransactionTraceIdentifier newtransactionID)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                string currentId = string.Empty;
                string newId = string.Empty;
                if (currenttransactionID.TransactionIdentifier != null)
                {
                    currentId = currenttransactionID.TransactionIdentifier.ToString();
                }
                if (newtransactionID.TransactionIdentifier != null)
                {
                    newId = newtransactionID.TransactionIdentifier.ToString();
                }
                TransactionScopeCurrentChanged(currentId, newId);
            }
        }

        [Event(TRANSACTIONSCOPE_CURRENT_CHANGED_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Warning, Task = Tasks.TransactionScope, Opcode = Opcodes.CurrentChanged, Message = "Transactionscope current transaction ID changed from {0} to {1}")]
        private void TransactionScopeCurrentChanged(string currenttransactionID, string newtransactionID)
        {
            SetActivityId(newtransactionID);
            WriteEvent(TRANSACTIONSCOPE_CURRENT_CHANGED_EVENTID, currenttransactionID, newtransactionID);
        }
        #endregion

        #region Transactionscope Nested Incorrectly
        /// <summary>Trace an event when a transactionscope is nested incorrectly.</summary>
        /// <param name="transactionID">The transaction ID.</param>
        [NonEvent]
        internal void TransactionScopeNestedIncorrectly(TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                TransactionScopeNestedIncorrectly(transactionID.TransactionIdentifier ?? string.Empty);
            }
        }

        [Event(TRANSACTIONSCOPE_NESTED_INCORRECTLY_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Warning, Task = Tasks.TransactionScope, Opcode = Opcodes.NestedIncorrectly, Message = "Transactionscope nested incorrectly: transaction ID is {0}")]
        private void TransactionScopeNestedIncorrectly(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTIONSCOPE_NESTED_INCORRECTLY_EVENTID, transactionID);
        }
        #endregion

        #region Transactionscope Disposed
        /// <summary>Trace an event when a transactionscope is disposed.</summary>
        /// <param name="transactionID">The transaction ID.</param>
        [NonEvent]
        internal void TransactionScopeDisposed(TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                TransactionScopeDisposed(transactionID.TransactionIdentifier ?? string.Empty);
            }
        }

        [Event(TRANSACTIONSCOPE_DISPOSED_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Informational, Task = Tasks.TransactionScope, Opcode = Opcodes.Disposed, Message = "Transactionscope disposed: transaction ID is {0}")]
        private void TransactionScopeDisposed(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTIONSCOPE_DISPOSED_EVENTID, transactionID);
        }
        #endregion

        #region Transactionscope Incomplete
        /// <summary>Trace an event when a transactionscope incomplete.</summary>
        /// <param name="transactionID">The transaction ID.</param>
        [NonEvent]
        internal void TransactionScopeIncomplete(TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                TransactionScopeIncomplete(transactionID.TransactionIdentifier ?? string.Empty);
            }
        }

        [Event(TRANSACTIONSCOPE_INCOMPLETE_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Warning, Task = Tasks.TransactionScope, Opcode = Opcodes.Incomplete, Message = "Transactionscope incomplete: transaction ID is {0}")]
        private void TransactionScopeIncomplete(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTIONSCOPE_INCOMPLETE_EVENTID, transactionID);
        }
        #endregion

        #region Transactionscope Timeout
        /// <summary>Trace an event when there is timeout on transactionscope.</summary>
        /// <param name="transactionID">The transaction ID.</param>
        [NonEvent]
        internal void TransactionScopeTimeout(TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                TransactionScopeTimeout(transactionID.TransactionIdentifier ?? string.Empty);
            }
        }

        [Event(TRANSACTIONSCOPE_TIMEOUT_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Warning, Task = Tasks.TransactionScope, Opcode = Opcodes.Timeout, Message = "Transactionscope timeout: transaction ID is {0}")]
        private void TransactionScopeTimeout(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTIONSCOPE_TIMEOUT_EVENTID, transactionID);
        }
        #endregion

        #region Transaction Timeout
        /// <summary>Trace an event when there is timeout on transaction.</summary>
        /// <param name="transactionID">The transaction ID.</param>
        [NonEvent]
        internal void TransactionTimeout(TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                TransactionTimeout(transactionID.TransactionIdentifier ?? string.Empty);
            }
        }

        [Event(TRANSACTION_TIMEOUT_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Warning, Task = Tasks.Transaction, Opcode = Opcodes.Timeout, Message = "Transaction timeout: transaction ID is {0}")]
        private void TransactionTimeout(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_TIMEOUT_EVENTID, transactionID);
        }
        #endregion

        #region Transaction Enlist
        /// <summary>Trace an event when there is enlist.</summary>
        /// <param name="enlistmentID">The enlistment ID.</param>
        /// <param name="enlistmentType">The enlistment type.</param>
        /// <param name="enlistmentOption">The enlistment option.</param>
        [NonEvent]
        internal void TransactionstateEnlist(EnlistmentTraceIdentifier enlistmentID, EnlistmentType enlistmentType, EnlistmentOptions enlistmentOption)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                if (enlistmentID.EnlistmentIdentifier != 0)
                    TransactionstateEnlist(enlistmentID.EnlistmentIdentifier.ToString(), enlistmentType.ToString(), enlistmentOption.ToString());
                else
                    TransactionstateEnlist(string.Empty, enlistmentType.ToString(), enlistmentOption.ToString());
            }
        }

        [Event(TRANSACTIONSTATE_ENLIST_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Informational, Task = Tasks.TransactionState, Opcode = Opcodes.Enlist, Message = "Transactionstate enlist: Enlistment ID is {0}, type is {1} and options is {2}")]
        private void TransactionstateEnlist(string enlistmentID, string type, string option)
        {
            SetActivityId(string.Empty);
            WriteEvent(TRANSACTIONSTATE_ENLIST_EVENTID, enlistmentID, type, option);
        }
        #endregion

        #region Transaction committed
        [NonEvent]
        internal void TransactionCommitted(TraceSourceType traceSource, TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Verbose, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionCommittedLtm(transactionID.TransactionIdentifier ?? string.Empty);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionCommittedOleTx(transactionID.TransactionIdentifier ?? string.Empty);
                }
            }
        }

        [Event(TRANSACTION_COMMITTED_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Verbose, Task = Tasks.Transaction, Opcode = Opcodes.Committed, Message = "Transaction committed LTM: transaction ID is {0}")]
        private void TransactionCommittedLtm(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_COMMITTED_LTM_EVENTID, transactionID);
        }
        [Event(TRANSACTION_COMMITTED_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Verbose, Task = Tasks.Transaction, Opcode = Opcodes.Committed, Message = "Transaction committed OleTx: transaction ID is {0}")]
        private void TransactionCommittedOleTx(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_COMMITTED_OLETX_EVENTID, transactionID);
        }
        #endregion

        #region Transaction indoubt
        [NonEvent]
        internal void TransactionInDoubt(TraceSourceType traceSource, TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionInDoubtLtm(transactionID.TransactionIdentifier ?? string.Empty);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionInDoubtOleTx(transactionID.TransactionIdentifier ?? string.Empty);
                }
            }
        }

        [Event(TRANSACTION_INDOUBT_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Warning, Task = Tasks.Transaction, Opcode = Opcodes.InDoubt, Message = "Transaction indoubt LTM: transaction ID is {0}")]
        private void TransactionInDoubtLtm(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_INDOUBT_LTM_EVENTID, transactionID);
        }
        [Event(TRANSACTION_INDOUBT_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Warning, Task = Tasks.Transaction, Opcode = Opcodes.InDoubt, Message = "Transaction indoubt OleTx: transaction ID is {0}")]
        private void TransactionInDoubtOleTx(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_INDOUBT_OLETX_EVENTID, transactionID);
        }
        #endregion

        #region Transactionstate promoted
        /// <summary>Trace an event when transaction is promoted.</summary>
        /// <param name="transactionID">The transaction ID.</param>
        /// <param name="distributedTxID">The distributed transaction ID.</param>
        [NonEvent]
        internal void TransactionPromoted(TransactionTraceIdentifier transactionID, TransactionTraceIdentifier distributedTxID)
        {
            if (IsEnabled(EventLevel.Informational, ALL_KEYWORDS))
            {
                TransactionPromoted(transactionID.TransactionIdentifier ?? string.Empty, distributedTxID.TransactionIdentifier ?? string.Empty);
            }
        }

        [Event(TRANSACTION_PROMOTED_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Informational, Task = Tasks.Transaction, Opcode = Opcodes.Promoted, Message = "Transaction promoted: transaction ID is {0} and distributed transaction ID is {1}")]
        private void TransactionPromoted(string transactionID, string distributedTxID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_PROMOTED_EVENTID, transactionID, distributedTxID);
        }
        #endregion

        #region Transaction aborted
        [NonEvent]
        internal void TransactionAborted(TraceSourceType traceSource, TransactionTraceIdentifier transactionID)
        {
            if (IsEnabled(EventLevel.Warning, ALL_KEYWORDS))
            {
                if (traceSource == TraceSourceType.TraceSourceLtm)
                {
                    TransactionAbortedLtm(transactionID.TransactionIdentifier ?? string.Empty);
                }
                else if (traceSource == TraceSourceType.TraceSourceOleTx)
                {
                    TransactionAbortedOleTx(transactionID.TransactionIdentifier ?? string.Empty);
                }
            }
        }

        [Event(TRANSACTION_ABORTED_LTM_EVENTID, Keywords = Keywords.TraceLtm, Level = EventLevel.Warning, Task = Tasks.Transaction, Opcode = Opcodes.Aborted, Message = "Transaction aborted LTM: transaction ID is {0}")]
        private void TransactionAbortedLtm(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_ABORTED_LTM_EVENTID, transactionID);
        }
        [Event(TRANSACTION_ABORTED_OLETX_EVENTID, Keywords = Keywords.TraceOleTx, Level = EventLevel.Warning, Task = Tasks.Transaction, Opcode = Opcodes.Aborted, Message = "Transaction aborted OleTx: transaction ID is {0}")]
        private void TransactionAbortedOleTx(string transactionID)
        {
            SetActivityId(transactionID);
            WriteEvent(TRANSACTION_ABORTED_OLETX_EVENTID, transactionID);
        }
        #endregion

        #region Internal Error
        /// <summary>Trace an event when there is an internal error.</summary>
        /// <param name="error">The error information.</param>
        [NonEvent]
        internal void InternalError(string? error = null)
        {
            if (IsEnabled(EventLevel.Critical, ALL_KEYWORDS))
            {
                InternalErrorTrace(error);
            }
        }

        [Event(INTERNAL_ERROR_EVENTID, Keywords = Keywords.TraceBase, Level = EventLevel.Critical, Task = Tasks.TransactionScope, Opcode = Opcodes.InternalError, Message = "Transactionscope internal error: {0}")]
        private void InternalErrorTrace(string? error)
        {
            SetActivityId(string.Empty);
            WriteEvent(INTERNAL_ERROR_EVENTID, error);
        }
        #endregion

        public static class Opcodes
        {
            public const EventOpcode Aborted = (EventOpcode)100;
            public const EventOpcode Activity = (EventOpcode)101;
            public const EventOpcode Adjusted = (EventOpcode)102;
            public const EventOpcode CloneCreate = (EventOpcode)103;
            public const EventOpcode Commit = (EventOpcode)104;
            public const EventOpcode Committed = (EventOpcode)105;
            public const EventOpcode Create = (EventOpcode)106;
            public const EventOpcode Created = (EventOpcode)107;
            public const EventOpcode CurrentChanged = (EventOpcode)108;
            public const EventOpcode DependentCloneComplete = (EventOpcode)109;
            public const EventOpcode Disposed = (EventOpcode)110;
            public const EventOpcode Done = (EventOpcode)111;
            public const EventOpcode Enlist = (EventOpcode)112;
            public const EventOpcode Enter = (EventOpcode)113;
            public const EventOpcode ExceptionConsumed = (EventOpcode)114;
            public const EventOpcode Exit = (EventOpcode)115;
            public const EventOpcode ForceRollback = (EventOpcode)116;
            public const EventOpcode Incomplete = (EventOpcode)117;
            public const EventOpcode InDoubt = (EventOpcode)118;
            public const EventOpcode InternalError = (EventOpcode)119;
            public const EventOpcode InvalidOperation = (EventOpcode)120;
            public const EventOpcode NestedIncorrectly = (EventOpcode)121;
            public const EventOpcode Prepared = (EventOpcode)122;
            public const EventOpcode Promoted = (EventOpcode)123;
            public const EventOpcode RecoveryComplete = (EventOpcode)124;
            public const EventOpcode Reenlist = (EventOpcode)125;
            public const EventOpcode Rollback = (EventOpcode)126;
            public const EventOpcode Serialized = (EventOpcode)127;
            public const EventOpcode Timeout = (EventOpcode)128;
            public const EventOpcode CallbackPositive = (EventOpcode)129;
            public const EventOpcode CallbackNegative = (EventOpcode)130;
        }

        public static class Tasks
        {
            public const EventTask ConfiguredDefaultTimeout = (EventTask)1;
            public const EventTask Enlistment = (EventTask)2;
            public const EventTask ResourceManager = (EventTask)3;
            public const EventTask Method = (EventTask)4;
            public const EventTask Transaction = (EventTask)5;
            public const EventTask TransactionException = (EventTask)6;
            public const EventTask TransactionManager = (EventTask)7;
            public const EventTask TransactionScope = (EventTask)8;
            public const EventTask TransactionState = (EventTask)9;
        }

        public static class Keywords
        {
            public const EventKeywords TraceBase = (EventKeywords)0x0001;
            public const EventKeywords TraceLtm = (EventKeywords)0x0002;
            public const EventKeywords TraceOleTx = (EventKeywords)0x0004;
        }

        private static void SetActivityId(string str)
        {
            Guid guid = Guid.Empty;

            if (str.Contains('-'))
            { // GUID with dash
                if (str.Length >= 36)
                {
                    Guid.TryParse(str.AsSpan(0, 36), out guid);
                }
            }
            else
            {
                if (str.Length >= 32)
                {
                    Guid.TryParse(str.AsSpan(0, 32), out guid);
                }
            }
            SetCurrentThreadActivityId(guid);
        }
    }
}
