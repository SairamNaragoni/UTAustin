#[macro_use]
extern crate log;
extern crate stderrlog;
extern crate clap;
extern crate ctrlc;
extern crate ipc_channel;
use std::env;
use std::fs;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::process::{Child,Command};
use ipc_channel::ipc::IpcSender as Sender;
use ipc_channel::ipc::IpcReceiver as Receiver;
use ipc_channel::ipc::IpcOneShotServer;
use ipc_channel::ipc::channel;
pub mod message;
pub mod oplog;
pub mod coordinator;
pub mod participant;
pub mod client;
pub mod checker;
pub mod tpcoptions;
use message::ProtocolMessage;

///
/// pub fn spawn_child_and_connect(child_opts: &mut tpcoptions::TPCOptions) -> (std::process::Child, Sender<ProtocolMessage>, Receiver<ProtocolMessage>)
///
///     child_opts: CLI options for child process
///
/// 1. Set up IPC
/// 2. Spawn a child process using the child CLI options
/// 3. Do any required communication to set up the parent / child communication channels
/// 4. Return the child process handle and the communication channels for the parent
///
/// HINT: You can change the signature of the function if necessary
///
fn spawn_child_and_connect(child_opts: &mut tpcoptions::TPCOptions, child_tx:Sender<ProtocolMessage>, child_rx:Receiver<ProtocolMessage>) -> Result<Child, std::io::Error> {
    let (server, server_name) = IpcOneShotServer::<Sender<(Sender<ProtocolMessage>, Receiver<ProtocolMessage>)>>::new().unwrap();
    child_opts.ipc_path = server_name;

    let child = Command::new(env::current_exe().unwrap())
        .args(child_opts.as_vec())
        .spawn();

    let child = match child {
        Ok(c) => c,
        Err(e) => return Err(e),
    };

    let (_, tx_rx_sender) = server.accept().unwrap();
    tx_rx_sender.send((child_tx, child_rx)).unwrap();
    Ok(child)
}

///
/// pub fn connect_to_coordinator(opts: &tpcoptions::TPCOptions) -> (Sender<ProtocolMessage>, Receiver<ProtocolMessage>)
///
///     opts: CLI options for this process
///
/// 1. Connect to the parent via IPC
/// 2. Do any required communication to set up the parent / child communication channels
/// 3. Return the communication channels for the child
///
/// HINT: You can change the signature of the function if necessasry
///
fn connect_to_coordinator(opts: &tpcoptions::TPCOptions) -> (Sender<ProtocolMessage>, Receiver<ProtocolMessage>) {
    let socket = opts.ipc_path.clone();
    let sender = Sender::connect(socket).unwrap();
    let (tx_rx_sender, tx_rx_receiver) = channel().unwrap();

    sender.send(tx_rx_sender).unwrap();
    let (child_tx, child_rx) = tx_rx_receiver.recv().unwrap();

    (child_tx, child_rx)
}

///
/// pub fn run(opts: &tpcoptions:TPCOptions, running: Arc<AtomicBool>)
///     opts: An options structure containing the CLI arguments
///     running: An atomically reference counted (ARC) AtomicBool(ean) that is
///         set to be false whenever Ctrl+C is pressed
///
/// 1. Creates a new coordinator
/// 2. Spawns and connects to new clients processes and then registers them with
///    the coordinator
/// 3. Spawns and connects to new participant processes and then registers them
///    with the coordinator
/// 4. Starts the coordinator protocol
/// 5. Wait until the children finish execution
///
fn run(opts: & tpcoptions::TPCOptions, running: Arc<AtomicBool>) {
    let coord_log_path = format!("{}//{}", opts.log_path, "coordinator.log");

    let mut coord = coordinator::Coordinator::new(coord_log_path, &running);

    for participant_id in 0..opts.num_participants {
        let participant_name = format!("participant_{}", participant_id);
        let mut participant_opts = opts.clone();
        participant_opts.mode = String::from("participant");
        participant_opts.num = participant_id;
        let (participant_tx, participant_rx) = coord.participant_join(&participant_name);
        match spawn_child_and_connect(&mut participant_opts, participant_tx, participant_rx) {
            Ok(child) => {
                info!("spawned participant process with PID={}, name={}", child.id(), participant_name);
            }
            Err(e) => {
                // Handle the error, remove participant from the coordinator
                info!("failed to spawn participant {}: {:?}", participant_name, e);
                coord.participant_leave(&participant_name);
            }
        }
    }

    for client_id in 0..opts.num_clients {
        let client_name: String = format!("client_{}", client_id);
        let mut client_opts = opts.clone();
        client_opts.mode = String::from("client");
        client_opts.num = client_id;
        let (client_tx, client_rx) = coord.client_join(&client_name);
        match spawn_child_and_connect(&mut client_opts, client_tx, client_rx) {
            Ok(child) => {
                info!("spawned client process with PID={}, name={}", child.id(), client_name);
            }
            Err(e) => {
                // Handle the error, remove client from the coordinator
                info!("failed to spawn participant {}: {:?}", client_name, e);
                coord.client_leave(&client_name);
            }
        }
    }

    coord.protocol();

}

///
/// pub fn run_client(opts: &tpcoptions:TPCOptions, running: Arc<AtomicBool>)
///     opts: An options structure containing the CLI arguments
///     running: An atomically reference counted (ARC) AtomicBool(ean) that is
///         set to be false whenever Ctrl+C is pressed
///
/// 1. Connects to the coordinator to get tx/rx
/// 2. Constructs a new client
/// 3. Starts the client protocol
///
fn run_client(opts: & tpcoptions::TPCOptions, running: Arc<AtomicBool>) {
    let (client_tx, client_rx) = connect_to_coordinator(opts);
    let client_name: String = format!("client_{}", opts.num);

    let mut client = client::Client::new(client_name, running, client_tx, client_rx);
    client.protocol(opts.num_requests);
}

///
/// pub fn run_participant(opts: &tpcoptions:TPCOptions, running: Arc<AtomicBool>)
///     opts: An options structure containing the CLI arguments
///     running: An atomically reference counted (ARC) AtomicBool(ean) that is
///         set to be false whenever Ctrl+C is pressed
///
/// 1. Connects to the coordinator to get tx/rx
/// 2. Constructs a new participant
/// 3. Starts the participant protocol
///
fn run_participant(opts: & tpcoptions::TPCOptions, running: Arc<AtomicBool>) {
    let participant_name = format!("participant_{}", opts.num);
    let participant_log_path = format!("{}//{}.log", opts.log_path, participant_name);
    let (participant_tx, participant_rx) = connect_to_coordinator(opts);
    let mut participant = participant::Participant::new(
        participant_name,
        participant_log_path,
        running,
        opts.send_success_probability,
        opts.operation_success_probability,
        participant_tx,
        participant_rx,
    );
    participant.protocol();
}

fn main() {
    // Parse CLI arguments
    let opts = tpcoptions::TPCOptions::new();
    // Set-up logging and create OpLog path if necessary
    stderrlog::new()
            .module(module_path!())
            .quiet(false)
            .timestamp(stderrlog::Timestamp::Millisecond)
            .verbosity(opts.verbosity)
            .init()
            .unwrap();
    match fs::create_dir_all(opts.log_path.clone()) {
        Err(e) => error!("Failed to create log_path: \"{:?}\". Error \"{:?}\"", opts.log_path, e),
        _ => (),
    }

    // Set-up Ctrl-C / SIGINT handler
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();
    let m = opts.mode.clone();
    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
        if m == "run" {
            print!("\n");
        }
    }).expect("Error setting signal handler!");

    // Execute main logic
    match opts.mode.as_ref() {
        "run" => run(&opts, running),
        "client" => run_client(&opts, running),
        "participant" => run_participant(&opts, running),
        "check" => checker::check_last_run(opts.num_clients, opts.num_requests, opts.num_participants, &opts.log_path),
        _ => panic!("Unknown mode"),
    }
}
