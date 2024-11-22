//!
//! coordinator.rs
//! Implementation of 2PC coordinator
//!
extern crate log;
extern crate stderrlog;
extern crate rand;
extern crate ipc_channel;

use std::collections::HashMap;
use std::sync::mpsc;
use std::sync::Arc;
use std::sync::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::time::Duration;
use std::time::Instant;

use coordinator::ipc_channel::ipc::IpcSender as Sender;
use coordinator::ipc_channel::ipc::IpcReceiver as Receiver;
use coordinator::ipc_channel::ipc::TryRecvError;
use coordinator::ipc_channel::ipc::channel;

use message;
use message::MessageType;
use message::ProtocolMessage;
use message::RequestStatus;
use oplog;

use crate::coordinator;

const COORDINATOR_ID: &str = "coordinator";
const COORDINATOR_EXIT_TIMEOUT: Duration = Duration::from_millis(100);
const COORDINATOR_VOTE_TIMEOUT: Duration = Duration::from_millis(25);


/// CoordinatorState
/// States for 2PC state machine
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CoordinatorState {
    Quiescent,
    ReceivedRequest,
    ProposalSent,
    ReceivedVotesAbort,
    ReceivedVotesCommit,
    SentGlobalDecision
}

/// Coordinator
/// Struct maintaining state for coordinator
#[derive(Debug)]
pub struct Coordinator {
    state: CoordinatorState,
    running: Arc<AtomicBool>,
    log: oplog::OpLog,
    participants: HashMap<String, (Sender<ProtocolMessage>, Receiver<ProtocolMessage>)>,
    clients: HashMap<String, (Sender<ProtocolMessage>, Receiver<ProtocolMessage>)>,
    successful_ops: usize,
    failed_ops: usize,
    unknown_ops: usize,
}

///
/// Coordinator
/// Implementation of coordinator functionality
/// Required:
/// 1. new -- Constructor
/// 2. protocol -- Implementation of coordinator side of protocol
/// 3. report_status -- Report of aggregate commit/abort/unknown stats on exit.
/// 4. participant_join -- What to do when a participant joins
/// 5. client_join -- What to do when a client joins
///
impl Coordinator {

    ///
    /// new()
    /// Initialize a new coordinator
    ///
    /// <params>
    ///     log_path: directory for log files --> create a new log there.
    ///     r: atomic bool --> still running?
    ///
    pub fn new(
        log_path: String,
        r: &Arc<AtomicBool>) -> Coordinator {

        Coordinator {
            state: CoordinatorState::Quiescent,
            log: oplog::OpLog::new(log_path),
            running: r.clone(),
            participants: HashMap::new(),
            clients: HashMap::new(),
            successful_ops: 0,
            failed_ops: 0,
            unknown_ops: 0,
        }
    }

    ///
    /// participant_join()
    /// Adds a new participant for the coordinator to keep track of
    ///
    /// HINT: Keep track of any channels involved!
    /// HINT: You may need to change the signature of this function
    ///
    pub fn participant_join(&mut self, name: &String) -> (Sender<ProtocolMessage>, Receiver<ProtocolMessage>) {
        assert!(self.state == CoordinatorState::Quiescent);

        let (local_tx, remote_rx) = channel().unwrap();
        let (remote_tx, local_rx) = channel().unwrap();

        self.participants.insert(name.clone(), (local_tx, local_rx));
        info!("coordinator::participant_join -> participant {} joined", name);
        (remote_tx, remote_rx)
    }

    pub fn participant_leave(&mut self, name: &String) {
        self.participants.remove(name);
        info!("coordinator::participant_leave -> participant {} left", name);
    }

    ///
    /// client_join()
    /// Adds a new client for the coordinator to keep track of
    ///
    /// HINT: Keep track of any channels involved!
    /// HINT: You may need to change the signature of this function
    ///
    pub fn client_join(&mut self, name: &String) -> (Sender<ProtocolMessage>, Receiver<ProtocolMessage>) {
        assert!(self.state == CoordinatorState::Quiescent);
        let (local_tx, remote_rx) = channel().unwrap();
        let (remote_tx, local_rx) = channel().unwrap();

        self.clients.insert(name.clone(), (local_tx, local_rx));
        info!("coordinator::client_join -> client {} joined", name);
        (remote_tx, remote_rx)
    }

    pub fn client_leave(&mut self, name: &String) {
        self.clients.remove(name);
        info!("coordinator::client_leave -> client {} left", name);
    }

    ///
    /// report_status()
    /// Report the abort/commit/unknown status (aggregate) of all transaction
    /// requests made by this coordinator before exiting.
    ///
    pub fn report_status(&mut self) {
        println!("coordinator     :\tCommitted: {:6}\tAborted: {:6}\tUnknown: {:6}", self.successful_ops, self.failed_ops, self.unknown_ops);
    }

    fn receive_client_requests(&mut self) -> Vec<ProtocolMessage> {

        let client_requests = self.clients.values().filter_map(| (_, rx)|{
            rx.try_recv().ok()
        }).collect();

        trace!(
            "coordinator::receive_client_requests -> requests received from clients, no_of_clients={}, requests={:?}"
            , self.clients.len(), client_requests
        );

        client_requests
    }

    fn send_prepare(&mut self, protocol_message: &ProtocolMessage) {
        debug!("coordinator::send_prepare -> prepare request for {:?}", protocol_message);

        let coordinator_pm = ProtocolMessage::generate(
            MessageType::CoordinatorPropose, 
            protocol_message.txid.clone(), 
            COORDINATOR_ID.to_string(), 
            protocol_message.opid.clone());
        
        self.log.append_message(coordinator_pm.clone());

        self.participants.retain(|name, (tx, _)| {
            match tx.send(coordinator_pm.clone()) {
                Ok(_) => true,
                Err(err) => {
                    info!("coordinator::send_prepare -> {} participant err/disconnected - {:?}", name, err);
                    false
                }
            }
        });
    }

    fn collect_votes(&mut self, protocol_message: &ProtocolMessage) -> RequestStatus{
        debug!("coordinator::collect_votes -> collect votes from participants for {:?}", protocol_message);

        let mut vote_status = RequestStatus::Committed;
    
        // // Iterate over all participants to collect votes
        // for (name, (_, rx)) in &self.participants {
        //     let start_time = Instant::now();
        //     loop {
        //         let elapsed = start_time.elapsed();

        //         if elapsed > COORDINATOR_VOTE_TIMEOUT {
        //             warn!("coordinator::collect_votes -> timeout waiting for vote from participant {} with txid={}", name, protocol_message.txid.clone());
        //             vote_status = RequestStatus::Unknown;
        //             break;
        //         }

        //         match rx.try_recv() {
        //           Ok(vote_message)  if vote_message.txid == protocol_message.txid => {
        //             vote_status = match vote_message.mtype {
        //                 MessageType::ParticipantVoteCommit => vote_status,
        //                 MessageType::ParticipantVoteAbort => RequestStatus::Aborted,
        //                 _ => unreachable!(),
        //             };
        //             break;
        //           },
        //           Ok(vote_message) if vote_message.txid != protocol_message.txid => {
        //             error!("coordinator::collect_votes -> txid mismatch for participant={}, expected_txid={}, received_txid={}", name, protocol_message.txid.clone(), vote_message.txid.clone());
        //           },
        //           Ok(_) => unreachable!(),
        //           Err(err) => {
        //             trace!("coordinator::collect_votes -> failed to receive vote from participant {}: {:?}", name, err);
        //           },
        //         };
        //     }
        // }

        // Iterate over all participants to collect votes
        for (name, (_, rx)) in &self.participants {
            loop {
                match rx.try_recv_timeout(COORDINATOR_VOTE_TIMEOUT) {
                    Ok(vote_message) if vote_message.txid == protocol_message.txid => {
                        vote_status = match vote_message.mtype {
                            MessageType::ParticipantVoteCommit => vote_status,
                            MessageType::ParticipantVoteAbort => RequestStatus::Aborted,
                            _ => unreachable!(),
                        };
                        break;
                    },
                    Ok(vote_message) if vote_message.txid != protocol_message.txid => {
                        error!("coordinator::collect_votes -> txid mismatch for participant={}, expected_txid={}, received_txid={}", name, protocol_message.txid.clone(), vote_message.txid.clone());
                    },
                    Ok(_) => unreachable!(),
                    Err(err) => {
                        error!("coordinator::collect_votes -> failed to receive vote from participant={}, txid={}: {:?}", name, protocol_message.txid.clone(), err);
                        vote_status = RequestStatus::Unknown;
                        break;
                    },
                };
            }
        }

        info!("coordinator::collect_votes -> participant vote results = {:?}, txid={}", vote_status, protocol_message.txid.clone());
        vote_status
    }


    pub fn coordinator_commit(&mut self, protocol_message: &ProtocolMessage, vote_status: &RequestStatus) -> (ProtocolMessage, ProtocolMessage) {
        let coordinator_commit_status = match vote_status {
            RequestStatus::Aborted => MessageType::CoordinatorAbort,
            RequestStatus::Committed => MessageType::CoordinatorCommit,
            RequestStatus::Unknown => MessageType::CoordinatorAbort,
        };

        let coordinator_message = ProtocolMessage::generate(
            coordinator_commit_status, 
            protocol_message.txid.clone(), 
            COORDINATOR_ID.to_string(), 
            protocol_message.opid,
        );

        self.log.append_message(coordinator_message.clone());
        debug!("coordinator::coordinator_commit -> coordinator commit status {:?}", coordinator_message);

        let client_commit_status = match coordinator_commit_status {
            MessageType::CoordinatorAbort => {
                self.failed_ops += 1;
                MessageType::ClientResultAbort
            },
            MessageType::CoordinatorCommit => {
                self.successful_ops += 1;
                MessageType::ClientResultCommit
            },
            _ => unreachable!(),
        };

        let client_message = ProtocolMessage::generate(
            client_commit_status, 
            protocol_message.txid.clone(), 
            COORDINATOR_ID.to_string(), 
            protocol_message.opid
        );

        (coordinator_message, client_message)
    }


    fn participant_commit(&mut self, coordinator_message: ProtocolMessage) {
        debug!("coordinator::participant_commit -> participant message {:?}", coordinator_message);

        self.participants.retain(|name, (tx, _)| {
            match tx.send(coordinator_message.clone()) {
                Ok(_) => true,
                Err(err) => {
                    info!("coordinator::participant_commit -> {} participant err/disconnected - {:?}", name, err);
                    false
                }
            }
        });
    }

    fn respond_to_clients(&mut self, request: &ProtocolMessage, client_response: ProtocolMessage) {
        debug!("coordinator::respond_to_clients -> client response message={:?}", client_response);
        let sender_id = &request.senderid;
        let (tx, _) = self.clients.get(sender_id).unwrap();
        match tx.send(client_response) {
            Ok(_) => (),
            Err(err) => {
                info!("coordinator::respond_to_clients -> {} client err/disconnected - {:?}", sender_id, err);
                self.client_leave(sender_id);
            }
        }
    }

    fn send_exit_signals(&mut self) {
        let exit_message = ProtocolMessage::generate(
            MessageType::CoordinatorExit,
            "-1".to_string(),
            COORDINATOR_ID.to_string(),
            0,
        );
        
        for (tx, _) in self.participants.values() {
            tx.send(exit_message.clone()).ok();
        }

        for (tx, _) in self.clients.values() {
            tx.send(exit_message.clone()).ok();
        }

        while self.running.load(Ordering::SeqCst) {
            thread::sleep(COORDINATOR_EXIT_TIMEOUT);
        }

        debug!("coordinator::send_exit_signals -> coordinator exiting !!")
    }

    ///
    /// protocol()
    /// Implements the coordinator side of the 2PC protocol
    /// HINT: If the simulation ends early, don't keep handling requests!
    /// HINT: Wait for some kind of exit signal before returning from the protocol!
    ///
    pub fn protocol(&mut self) {

        while self.running.load(Ordering::SeqCst) {
            let client_requests = self.receive_client_requests();
            
            for request in client_requests {
                //phase1 - PREPARE
                self.send_prepare(&request);
                let vote_status = self.collect_votes(&request);

                //phase2 - COMMIT
                let (coordinator_message, client_message) = self.coordinator_commit(&request, &vote_status);
                self.participant_commit(coordinator_message);
                self.respond_to_clients(&request, client_message);
            }
        }

        //send exit status
        self.send_exit_signals();
        self.report_status();
    }
}
