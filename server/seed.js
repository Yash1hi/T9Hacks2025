import PocketBase from 'pocketbase';

const pb = new PocketBase('http://127.0.0.1:8090');

const defaultMedications = [
  {
    name: "Vitamin D",
    description: "Daily vitamin D supplement",
    standardDosage: "1000 IU"
  },
  {
    name: "Vitamin C",
    description: "Immune system support",
    standardDosage: "500mg"
  },
  {
    name: "Multivitamin",
    description: "Daily multivitamin supplement",
    standardDosage: "1 tablet"
  },
  {
    name: "Fish Oil",
    description: "Omega-3 fatty acids supplement",
    standardDosage: "1000mg"
  },
  {
    name: "Melatonin",
    description: "Sleep support supplement",
    standardDosage: "5mg"
  }
];

async function seedDatabase() {
  try {
    // First authenticate as admin
    await pb.admins.authWithPassword('your-admin-email', 'your-admin-password');

    // Add each medication
    for (const med of defaultMedications) {
      await pb.collection('medications').create(med);
    }

    console.log('Database seeded successfully!');
  } catch (error) {
    console.error('Error seeding database:', error);
  }
}

seedDatabase(); 