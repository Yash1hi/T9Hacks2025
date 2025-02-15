// Users collection is already built-in with PocketBase

module.exports = [
  // Medications Collection
  {
    name: "medications",
    fields: [
      {
        name: "name",
        type: "text",
        required: true
      },
      {
        name: "description",
        type: "text"
      },
      {
        name: "standardDosage",
        type: "text"
      }
    ]
  },

  // User Medications Collection
  {
    name: "user_medications",
    fields: [
      {
        name: "user",
        type: "relation",
        required: true,
        options: {
          collectionId: "users"
        }
      },
      {
        name: "medication",
        type: "relation",
        required: true,
        options: {
          collectionId: "medications"
        }
      },
      {
        name: "dosage",
        type: "text",
        required: true
      },
      {
        name: "timeOfDay",
        type: "select",
        required: true,
        options: {
          values: ["morning", "evening"]
        }
      }
    ]
  },

  // Medication Logs Collection
  {
    name: "medication_logs",
    fields: [
      {
        name: "userMedication",
        type: "relation",
        required: true,
        options: {
          collectionId: "user_medications"
        }
      },
      {
        name: "takenAt",
        type: "date",
        required: true
      },
      {
        name: "wasTaken",
        type: "bool",
        required: true
      }
    ]
  }
]; 