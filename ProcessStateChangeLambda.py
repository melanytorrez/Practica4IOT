import json
import boto3
import logging

# Configuración del logger
logger = logging.getLogger()
logger.setLevel(logging.INFO)

# Inicialización de DynamoDB
dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('ActuatorStateChanges')

def lambda_handler(event, context):
    logger.info(f"Evento recibido: {json.dumps(event)}")
    
    try:
        # El evento de la regla de IoT para /documents tiene esta estructura
        thing_name = event.get('thing_name')
        timestamp = event.get('timestamp')
        
        # Obtenemos los estados actual y previo
        current_reported = event.get('current', {}).get('state', {}).get('reported', {})
        previous_reported = event.get('previous', {}).get('state', {}).get('reported', {})

        if not thing_name or not timestamp or not current_reported:
            logger.warning("Evento incompleto, no se puede procesar.")
            return

        # Iteramos sobre todos los actuadores en el estado actual
        for key, current_value in current_reported.items():
            previous_value = previous_reported.get(key)
            
            # Si el valor ha cambiado o es la primera vez que se reporta
            if current_value != previous_value:
                logger.info(f"Cambio detectado para '{key}': de '{previous_value}' a '{current_value}'")
                
                # Construimos el ID único del actuador
                actuator_id = f"{thing_name}_{key}"
                
                # Extraemos el tipo y la localización del nombre de la clave
                # Ej: "luz_sala" -> tipo="luz", loc="sala"
                parts = key.split('_')
                actuator_type = parts[0] if len(parts) > 0 else "unknown"
                location = parts[1] if len(parts) > 1 else "general"

                # Creamos el ítem para DynamoDB
                item_to_save = {
                    'actuator_id': actuator_id,
                    'timestamp': timestamp,
                    'thing_name': thing_name,
                    'actuator_type': actuator_type,
                    'location': location,
                    'new_state': str(current_value) # Aseguramos que sea string
                }
                
                # Guardamos el ítem en la tabla
                table.put_item(Item=item_to_save)
                logger.info(f"Ítem guardado en ActuatorStateChanges: {json.dumps(item_to_save)}")

        return {
            'statusCode': 200,
            'body': json.dumps('Procesado exitosamente!')
        }

    except Exception as e:
        logger.error(f"Error procesando el evento: {e}")
        raise e