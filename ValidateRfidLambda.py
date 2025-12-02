import json
import boto3
import base64 # <-- AÑADIR ESTA LIBRERÍA

# Inicializar clientes de AWS
dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('ValidRfidCards')
iot_client = boto3.client('iot-data')

def lambda_handler(event, context):
    print(f"Evento recibido de la regla de IoT: {json.dumps(event)}")

    # --- LÓGICA DE DECODIFICACIÓN ---
    # El payload real viene codificado en Base64
    # La estructura exacta del 'event' puede variar, así que probamos las formas comunes
    
    payload_decoded = {}
    if isinstance(event, dict):
        # A menudo, el payload está en una clave como 'payload' o directamente en el evento
        # y necesita ser decodificado si es un string base64.
        # En este caso, con SELECT *, el evento es el payload mismo.
        payload_decoded = event
    else:
        # Si el evento es un string codificado en base64 (otro caso común)
        try:
            payload_decoded = json.loads(base64.b64decode(event).decode('utf-8'))
        except Exception as e:
            print(f"No se pudo decodificar el payload: {e}")
            return {'statusCode': 400, 'body': 'Error decodificando payload'}

    card_uid = payload_decoded.get('card_uid')
    
    if not card_uid:
        print(f"Error: 'card_uid' no encontrado en el payload decodificado: {json.dumps(payload_decoded)}")
        return {
            'statusCode': 400,
            'body': json.dumps('card_uid not found in event payload')
        }
    
    print(f"UID de tarjeta extraído: {card_uid}")
        
    # --- Lógica de Validación (sin cambios) ---
    response_status = "INVALID"
    try:
        response = table.get_item(Key={'card_uid': card_uid})
        if 'Item' in response:
            response_status = "VALID"
        print(f"Resultado de la validación en DynamoDB: {response_status}")
            
    except Exception as e:
        print(f"Error al consultar DynamoDB: {e}")
        response_status = "INVALID"

    # --- Enviar Respuesta de Vuelta al ESP32 (sin cambios) ---
    response_topic = 'MiCasa/rfid/checkResponse'
    response_payload = {
        'status': response_status,
        'card_uid': card_uid
    }
    
    try:
        iot_client.publish(
            topic=response_topic,
            qos=1,
            payload=json.dumps(response_payload)
        )
        print(f"Publicada respuesta en {response_topic}: {json.dumps(response_payload)}")
    except Exception as e:
        print(f"Error al publicar en AWS IoT: {e}")

    return {
        'statusCode': 200,
        'body': json.dumps(f'Processed card {card_uid}, result: {response_status}')
    }