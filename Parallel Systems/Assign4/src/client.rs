//!
//! client.rs
//! Implementation of 2PC client
//!
extern crate ipc_channel;
extern crate log;
extern crate stderrlog;

use std::thread;
use std::time::Duration;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::collections::HashMap;

use client::ipc_channel::ipc::IpcReceiver as Receiver;
use client::ipc_channel::ipc::TryRecvError;
use client::ipc_channel::ipc::IpcSender as Sender;

use message;
use message::MessageType;
use message::RequestStatus;
use message::ProtocolMessage;

use crate::coordinator;

const CLIENT_EXIT_TIMEOUT: Duration = Duration::from_millis(100);

// Client state and primitives for communicating with the coordinator
#[derive(Debug)]
pub struct Client {
    pub id_str: String,
    pub running: Arc<AtomicBool>,
    pub num_requests: u32,
    tx: Sender<ProtocolMessage>,
    rx: Receiver<ProtocolMessage>,
    successful_ops: usize,
    failed_ops: usize,
    unknown_ops: usize
}

///
/// Client Implementation
/// Required:
/// 1. new -- constructor
/// 2. pub fn report_status -- Reports number of committed/aborted/unknown
/// 3. pub fn protocol(&mut self, n_requests: i32) -- Implements client side protocol
///
impl Client {

    ///
    /// new()
    ///
    /// Constructs and returns a new client, ready to run the 2PC protocol
    /// with the coordinator.
    ///
    /// HINT: You may want to pass some channels or other communication
    ///       objects that enable coordinator->client and client->coordinator
    ///       messaging to this constructor.
    /// HINT: You may want to pass some global flags that indicate whether
    ///       the protocol is still running to this constructor
    ///
    pub fn new(id_str: String,
               running: Arc<AtomicBool>,
               tx: Sender<ProtocolMessage>,
               rx: Receiver<ProtocolMessage>
            ) -> Client {
                    Client {
                        id_str: id_str,
                        running: running,
                        num_requests: 0,
                        tx: tx,
                        rx: rx,
                        successful_ops: 0,
                        failed_ops: 0,
                        unknown_ops: 0
                    }
    }

    ///
    /// wait_for_exit_signal(&mut self)
    /// Wait until the running flag is set by the CTRL-C handler
    ///
    pub fn wait_for_exit_signal(&mut self) {
        debug!("{}::Waiting for exit signal", self.id_str.clone());

        while let Ok(result) = self.rx.recv() {
            if result.mtype == MessageType::CoordinatorExit {
                break;
            }
            thread::sleep(CLIENT_EXIT_TIMEOUT);
        }

        debug!("{}::Exiting", self.id_str.clone());
    }

    ///
    /// send_next_operation(&mut self)
    /// Send the next operation to the coordinator
    ///
    pub fn send_next_operation(&mut self) {

        // Create a new request with a unique TXID.
        self.num_requests = self.num_requests + 1;
        let txid = format!("{}_op_{}", self.id_str.clone(), self.num_requests);
        let pm = message::ProtocolMessage::generate(message::MessageType::ClientRequest,
                                                    txid.clone(),
                                                    self.id_str.clone(),
                                                    self.num_requests);
        info!("{}::Sending operation #{}", self.id_str.clone(), self.num_requests);

        self.tx.send(pm).unwrap();

        trace!("{}::Sent operation #{}", self.id_str.clone(), self.num_requests);
    }

    ///
    /// recv_result()
    /// Wait for the coordinator to respond with the result for the
    /// last issued request. Note that we assume the coordinator does
    /// not fail in this simulation
    ///
    pub fn recv_result(&mut self) -> bool {
        let txid = format!("{}_op_{}", self.id_str.clone(), self.num_requests);
        info!("{}::Receiving Coordinator Result", self.id_str.clone());
        let mut coordinator_exit = false;
        match self.rx.recv() {
            Ok(result) => {
                // Handle the received result from the coordinator
                match result.mtype {
                    MessageType::ClientResultCommit => {
                        debug!("client::recv_result -> {}::Received COMMIT for request #{}, #{}", self.id_str.clone(), result.txid, txid);
                        self.successful_ops += 1
                    },
                    MessageType::ClientResultAbort => {
                        debug!("client::recv_result -> {}::Received ABORT for request #{}, #{}", self.id_str.clone(), result.txid, txid);
                        self.failed_ops += 1
                    },
                    MessageType::CoordinatorExit => {
                        debug!("client::recv_result -> {}::Received Exit Signal from Coordinator", self.id_str.clone());
                        coordinator_exit = true;
                    },
                    _ => {
                        warn!("{}::Unexpected message type received: {:?}", self.id_str.clone(), result.mtype);
                        unreachable!()
                    }
                }
            },
            Err(recv_error) => {
                error!("client::recv_result -> {}::unknown error... {:?}", self.id_str.clone(), recv_error);
            }
        }
        coordinator_exit

    }

    ///
    /// report_status()
    /// Report the abort/commit/unknown status (aggregate) of all transaction
    /// requests made by this client before exiting.
    ///
    pub fn report_status(&mut self) {
        println!("{:16}:\tCommitted: {:6}\tAborted: {:6}\tUnknown: {:6}", self.id_str.clone(), self.successful_ops, self.failed_ops, self.unknown_ops);
    }

    ///
    /// protocol()
    /// Implements the client side of the 2PC protocol
    /// HINT: if the simulation ends early, don't keep issuing requests!
    /// HINT: if you've issued all your requests, wait for some kind of
    ///       exit signal before returning from the protocol method!
    ///
    pub fn protocol(&mut self, n_requests: u32) {

        let mut coordinator_exit = false;

        for _ in 0..n_requests {
            if !coordinator_exit {
                self.send_next_operation();
                coordinator_exit = self.recv_result();
            } 
        }

        info!("client::protocol -> requests finished for client={}", self.id_str);
        if !coordinator_exit{
            self.wait_for_exit_signal();
        }
        self.report_status();
    }
}
