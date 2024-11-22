//!
//! participant.rs
//! Implementation of 2PC participant
//!
extern crate ipc_channel;
extern crate log;
extern crate rand;
extern crate stderrlog;

use std::collections::HashMap;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;
use std::thread;

use participant::rand::prelude::*;
use participant::ipc_channel::ipc::IpcReceiver as Receiver;
use participant::ipc_channel::ipc::TryRecvError;
use participant::ipc_channel::ipc::IpcSender as Sender;

use message::MessageType;
use message::ProtocolMessage;
use message::RequestStatus;
use oplog;

use crate::participant;

const PARTICIPANT_EXIT_TIMEOUT: Duration = Duration::from_millis(100);

///
/// ParticipantState
/// enum for Participant 2PC state machine
///
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ParticipantState {
    Quiescent,
    ReceivedP1,
    VotedAbort,
    VotedCommit,
    AwaitingGlobalDecision,
}

///
/// Participant
/// Structure for maintaining per-participant state and communication/synchronization objects to/from coordinator
///
#[derive(Debug)]
pub struct Participant {
    id_str: String,
    state: ParticipantState,
    log: oplog::OpLog,
    running: Arc<AtomicBool>,
    send_success_prob: f64,
    operation_success_prob: f64,
    tx: Sender<ProtocolMessage>,
    rx: Receiver<ProtocolMessage>,
    successful_ops: usize,
    failed_ops: usize,
    unknown_ops: usize,
}

///
/// Participant
/// Implementation of participant for the 2PC protocol
/// Required:
/// 1. new -- Constructor
/// 2. pub fn report_status -- Reports number of committed/aborted/unknown for each participant
/// 3. pub fn protocol() -- Implements participant side protocol for 2PC
///
impl Participant {

    ///
    /// new()
    ///
    /// Return a new participant, ready to run the 2PC protocol with the coordinator.
    ///
    /// HINT: You may want to pass some channels or other communication
    ///       objects that enable coordinator->participant and participant->coordinator
    ///       messaging to this constructor.
    /// HINT: You may want to pass some global flags that indicate whether
    ///       the protocol is still running to this constructor. There are other
    ///       ways to communicate this, of course.
    ///
    pub fn new(
        id_str: String,
        log_path: String,
        r: Arc<AtomicBool>,
        send_success_prob: f64,
        operation_success_prob: f64,
        tx: Sender<ProtocolMessage>,
        rx: Receiver<ProtocolMessage>) -> Participant {

        Participant {
            id_str: id_str,
            state: ParticipantState::Quiescent,
            log: oplog::OpLog::new(log_path),
            running: r,
            send_success_prob: send_success_prob,
            operation_success_prob: operation_success_prob,
            tx: tx,
            rx: rx,
            successful_ops: 0,
            failed_ops: 0,
            unknown_ops: 0
        }
    }

    ///
    /// send()
    /// Send a protocol message to the coordinator. This can fail depending on
    /// the success probability. For testing purposes, make sure to not specify
    /// the -S flag so the default value of 1 is used for failproof sending.
    ///
    /// HINT: You will need to implement the actual sending
    ///
    pub fn send(&mut self, pm: ProtocolMessage) -> bool {
        let x: f64 = random();
        if x <= self.send_success_prob {
            if let Err(e) = self.tx.send(pm.clone()) {
                warn!("participant::send -> {}::failed to send message: {:?}", self.id_str.clone(), e);
                false
            } else {
                debug!("participant::send -> {}::successfully sent message: {:?}", self.id_str.clone(), pm);
                true
            }
        } else {
            warn!("participant::send -> {}::failed to send message due to probability failure", self.id_str.clone());
            false
        }
    }

    ///
    /// perform_operation
    /// Perform the operation specified in the 2PC proposal,
    /// with some probability of success/failure determined by the
    /// command-line option success_probability.
    ///
    /// HINT: The code provided here is not complete--it provides some
    ///       tracing infrastructure and the probability logic.
    ///       Your implementation need not preserve the method signature
    ///       (it's ok to add parameters or return something other than
    ///       bool if it's more convenient for your design).
    ///
    pub fn perform_operation(&mut self, request: &ProtocolMessage) -> bool {
        trace!("participant::perform_operation -> {}::Performing operation", self.id_str.clone());

        let participant_state;
        
        let x: f64 = random();
        if x <= self.operation_success_prob {
            participant_state = MessageType::ParticipantVoteCommit;
        } else {
            participant_state = MessageType::ParticipantVoteAbort;
        }

        let participant_result = ProtocolMessage::generate(
            participant_state, 
            request.txid.clone(), 
            self.id_str.clone(), 
            request.opid
        );

        self.log.append_message(participant_result.clone());
        let send_status = self.send(participant_result.clone());
        info!("participant::perform_operation -> send_status={}, vote_status={:?}, txid={}", send_status, participant_state, request.txid.clone());
        send_status
    }

    ///
    /// report_status()
    /// Report the abort/commit/unknown status (aggregate) of all transaction
    /// requests made by this coordinator before exiting.
    ///
    pub fn report_status(&mut self) {
        println!("{:16}:\tCommitted: {:6}\tAborted: {:6}\tUnknown: {:6}", self.id_str.clone(), 
        self.successful_ops, self.failed_ops, self.unknown_ops);
    }

    ///
    /// wait_for_exit_signal(&mut self)
    /// Wait until the running flag is set by the CTRL-C handler
    ///
    pub fn wait_for_exit_signal(&mut self) {
        trace!("{}::Waiting for exit signal", self.id_str.clone());

        while self.running.load(Ordering::SeqCst) {
            thread::sleep(PARTICIPANT_EXIT_TIMEOUT);
        }

        trace!("{}::Exiting", self.id_str.clone());
    }

    ///
    /// protocol()
    /// Implements the participant side of the 2PC protocol
    /// HINT: If the simulation ends early, don't keep handling requests!
    /// HINT: Wait for some kind of exit signal before returning from the protocol!
    ///
    pub fn protocol(&mut self) {
        trace!("{}::Beginning protocol", self.id_str.clone());

        loop {
            let request = self.rx.recv().unwrap();

            match request.mtype {
                MessageType::CoordinatorPropose => {
                    debug!("participant::protocol -> {}::Received CoordinatorPropose from Coordinator #{}", self.id_str.clone(), request.txid);
                    self.unknown_ops += 1;
                    self.perform_operation(&request);
                },
                MessageType::CoordinatorCommit => {
                    debug!("participant::protocol -> {}::Received CoordinatorCommit from Coordinator #{}", self.id_str.clone(), request.txid);
                    self.unknown_ops -= 1;
                    self.successful_ops += 1;
                    self.log.append_message(request);
                },
                MessageType::CoordinatorAbort => {
                    debug!("participant::protocol -> {}::Received CoordinatorAbort from Coordinator", self.id_str.clone());
                    self.unknown_ops -= 1;
                    self.failed_ops += 1;
                    self.log.append_message(request);
                },
                MessageType::CoordinatorExit => {
                    debug!("participant::protocol -> {}::Received Exit Signal from Coordinator", self.id_str.clone());
                    break;
                },
                _ => {
                    warn!("participant::protocol -> {}::Unexpected message type received: {:?}", self.id_str.clone(), request.mtype);
                    unreachable!()
                },
            };
        }

        self.wait_for_exit_signal();
        self.report_status();
    }
}
