Required setup in Azure Pipelines:
- Project settings / Service connections: Add Azure Resource Manager connection 
  - Service Principal Authentication
  - connection name: azureResourceManagerConnection
- Pipeline / Edit / Variables / Add
  - add secret variable pat_token with the Personal Access Token (required for registering the Azure Pipelines Agent)

Required setup in Azure Portal:
- Azure Active Directory / App-Registrations / New App + Secret
- Subscription / IAM / Add role / select app registration
