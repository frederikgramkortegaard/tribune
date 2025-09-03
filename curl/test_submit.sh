#!/bin/bash

# Test submit endpoint with valid data
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/json" \
  -d '{
    "event_id": "test-event-123",
    "client_id": "client-node-001",
    "data": "sample_computation_result_abc123",
    "timestamp": 1693766400000
  }'

echo
