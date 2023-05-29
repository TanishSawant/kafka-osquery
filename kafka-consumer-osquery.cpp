
#include <iostream>
#include <cppkafka/cppkafka.h>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

using namespace std;
using namespace cppkafka;
using json = nlohmann::json;

int main() {
    // Create a configuration object
    Configuration config = {
        {"metadata.broker.list", "localhost:9092"}, {"group.id", "my_consumer_group"}
    };

    // Create a Kafka consumer instance
    Consumer consumer(config);
    cout << "Consumer created" << endl;

    // Set the topic to consume from
    string topic_name = "osquery_log_1";
    TopicPartitionList partitions = {
        TopicPartition(topic_name, 0)  // Assuming partition 0
    };
    consumer.assign(partitions);
    cout << "Subscribed to topic: " << topic_name << endl;

    // Create a connection to the PostgreSQL database
    pqxx::connection conn("dbname=postgres user=postgres password= hostaddr=127.0.0.1 port=5432");

    // Create a transaction
    pqxx::work txn(conn);

    // Execute a query to create/replace the table
    txn.exec(R"(
        CREATE TABLE osquery_logs (
            id SERIAL PRIMARY KEY,
            cmdline VARCHAR,
            cwd VARCHAR,
            name VARCHAR,
            parent VARCHAR,
            received_at TIMESTAMPTZ DEFAULT NOW()
        )
    )");

    // Commit the initial transaction
    txn.commit();

    // Poll for messages
    while (true) {
        // Wait for a message
        Message msg = consumer.poll();
        
        // Extract the message payload
        if (msg) {
            string cmdline = "";
            string cwd = "";
            string name = "";
            string parent = "";
            string payload = msg.get_payload();
            // cout << "Received message: " << payload << endl;
            // Parse the JSON payload
            json data = json::parse(payload);
            for(int i = 0; i<data.size(); i++){
                string cmdline = data["snapshot"][i]["cmdline"];
                string cwd = data["snapshot"][i]["cwd"];
                string name = data["snapshot"][i]["name"];
                string parent = data["snapshot"][i]["parent"];



                // Create a new transaction
                pqxx::work txn(conn);

                cout<<cwd<<endl;
                cout<<cmdline<<endl;
                // Insert the fields into the PostgreSQL table
                txn.exec_params(
                    "INSERT INTO osquery_logs (cmdline, cwd, name, parent) VALUES ($1, $2, $3, $4)",
                    cmdline, cwd, name, parent
                );
                cout<<"Inserted!"<<endl;

                // Commit the transaction
                txn.commit();
            }
            
        }
    }

    return 0;
}
